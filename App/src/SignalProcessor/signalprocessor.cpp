#include "signalprocessor.h"

#include <algorithm>
#include <stdexcept>
#include <cassert>

using namespace std;

#define fun() fun_shortcut()

SignalProcessor::SignalProcessor(DataFile* file, unsigned int memory, double /*bufferRatio*/) : dataFile(file)
{
	cl_int err;

	M = dataFile->getSamplingFrequency();
	offset = M;
	delay = M/2 - 1;
	padding = 4;
	blockSize = PROGRAM_OPTIONS->get("blockSize").as<unsigned int>() - offset;
	dataFileGpuCacheBlockSize = (blockSize + offset)*file->getChannelCount();
	processorCacheBlockSizeCL = (blockSize + offset + padding)*file->getChannelCount();
	//processorCacheBlockSizeGL = blockSize*montageRows

	// Ensure required sizes.
	if (M%4 || (blockSize + offset)%4)
	{
		throw runtime_error("SignalProcessor requires both the filter length and block length to be multiples of 4");
	}

	clContext = new OpenCLContext(PROGRAM_OPTIONS->get("platform").as<int>(),
								  PROGRAM_OPTIONS->get("device").as<int>(),
								  CL_DEVICE_TYPE_ALL);






	// Construct the dataFile cache.	
	unsigned int dataFileCacheBlockCount = 2*memory/dataFileGpuCacheBlockSize/sizeof(float); // 2* bigger than gpu buffer

	if (dataFileCacheBlockCount <= 0)
	{
		throw runtime_error("Not enough available memory for the dataFileCache");
	}

	dataFileCache.insert(dataFileCache.begin(), dataFileCacheBlockCount, nullptr);
	for (auto& e : dataFileCache)
	{
		e = new float[dataFileGpuCacheBlockSize];
	}

	dataFileCacheLogic = new PriorityCacheLogic(dataFileCacheBlockCount);
	dataFileCacheFillerThread = thread(&SignalProcessor::dataFileCacheFiller, this, &threadsStop);

	// Construct the gpu cache.
	unsigned int gpuCacheBlockCount = memory/dataFileGpuCacheBlockSize/sizeof(float);

	if (gpuCacheBlockCount <= 0)
	{
		throw runtime_error("Not enough available memory for the gpuCache");
	}

	gpuCacheQueue = clCreateCommandQueue(clContext->getCLContext(), clContext->getCLDevice(), CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
	checkErrorCode(err, CL_SUCCESS, "clCreateCommandQueue()");

	gpuCache.insert(gpuCache.begin(), gpuCacheBlockCount, nullptr);
	for (auto& e : gpuCache)
	{
		e = clCreateBuffer(clContext->getCLContext(), CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, dataFileGpuCacheBlockSize*sizeof(float), nullptr, &err);
		checkErrorCode(err, CL_SUCCESS, "clCreateBuffer()");
	}

	gpuCacheLogic = new PriorityCacheLogic(gpuCacheBlockCount);
	gpuCacheFillerThread = thread(&SignalProcessor::gpuCacheFiller, this, &threadsStop);

	// Construct the processor cache.
	unsigned int processorCacheSize = PROGRAM_OPTIONS->get("processorQueues").as<unsigned int>();

	processorCacheQueues.insert(processorCacheQueues.begin(), processorCacheSize, 0);
	processorCacheCLBuffers.insert(processorCacheCLBuffers.begin(), processorCacheSize, 0);
	processorCacheGLBuffers.insert(processorCacheGLBuffers.begin(), processorCacheSize, 0);
	processorCacheVertexArrays.insert(processorCacheVertexArrays.begin(), processorCacheSize, 0);

	fun()->glGenBuffers(processorCacheSize, processorCacheGLBuffers.data());
	fun()->glGenVertexArrays(processorCacheSize, processorCacheVertexArrays.data());

	for (unsigned int i = 0; i < processorCacheSize; ++i)
	{
		processorCacheQueues[i]  = clCreateCommandQueue(clContext->getCLContext(), clContext->getCLDevice(), 0, &err);
		checkErrorCode(err, CL_SUCCESS, "clCreateCommandQueue()");

		processorCacheCLBuffers[i] = clCreateBuffer(clContext->getCLContext(), CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, processorCacheBlockSizeCL*sizeof(float), nullptr, &err);
		checkErrorCode(err, CL_SUCCESS, "clCreateBuffer()");

		fun()->glBindVertexArray(processorCacheVertexArrays[i]);
		fun()->glBindBuffer(GL_ARRAY_BUFFER, processorCacheGLBuffers[i]);
		fun()->glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));
		fun()->glEnableVertexAttribArray(0);
		if (!REALLOCATE_BUFFER)
		{
			fun()->glBufferData(GL_ARRAY_BUFFER, processorCacheBlockSizeGL*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		}
	}

	fun()->glBindBuffer(GL_ARRAY_BUFFER, 0);
	fun()->glBindVertexArray(0);

	processorCacheLogic = new PriorityCacheLogic(processorCacheSize);
}

SignalProcessor::~SignalProcessor()
{
	// Join dataFileCacherFillerThread.
	threadsStop.store(true);

	thread t([this] ()
	{
		dataFileCacheFillerThread.join();
		gpuCacheFillerThread.join();
	});

	inCV.notify_all();
	dataFileGpuCV.notify_all();

	t.join();

	// Release resources.
	cl_int err;

	delete clContext;

	delete dataFileCacheLogic;
	delete gpuCacheLogic;
	delete processorCacheLogic;

	err = clReleaseCommandQueue(gpuCacheQueue);
	checkErrorCode(err, CL_SUCCESS, "clReleaseCommandQueue");

	for (auto& e : processorCacheQueues)
	{
		err = clReleaseCommandQueue(e);
		checkErrorCode(err, CL_SUCCESS, "clReleaseCommandQueue");
	}

	for (auto& e : dataFileCache)
	{
		delete[] e;
	}

	for (auto& e : gpuCache)
	{
		err = clReleaseMemObject(e);
		checkErrorCode(err, CL_SUCCESS, "clReleaseMemObject()");
	}

	for (auto& e : processorCacheCLBuffers)
	{
		err = clReleaseMemObject(e);
		checkErrorCode(err, CL_SUCCESS, "clReleaseMemObject()");
	}

	fun()->glDeleteBuffers(processorCacheGLBuffers.size(), processorCacheGLBuffers.data());
	fun()->glDeleteVertexArrays(processorCacheVertexArrays.size(), processorCacheVertexArrays.data());
}

SignalBlock SignalProcessor::getAnyBlock(const std::set<int>& indexSet)
{
	unique_lock<recursive_mutex> processorCacheLock(processorCacheMutex);

	cl_int err;

	unsigned int processorCacheIndex;
	int blockIndex;

	prepareBlocks(indexSet, -1);

	// Fill loop.
	while (processorCacheLogic->fill(&processorCacheIndex, &blockIndex))
	{
		processorCacheLock.unlock();

		unsigned int gpuCacheIndex;

		{
			unique_lock<recursive_mutex> gpuCacheLock(gpuCacheMutex);

			while (gpuCacheLogic->read(blockIndex, &gpuCacheIndex) == false)
			{
				gpuProcessorCV.wait(gpuCacheLock);
			}
		}

		processorCacheLock.lock();

		// queue filter
		// queue montage

		cl_event event = clCreateUserEvent(clContext->getCLContext(), &err);
		checkErrorCode(err, CL_SUCCESS, "clCreateUserEvent");

		cacheCallbackData* data = new cacheCallbackData {&processorCacheMutex, &gpuCacheMutex, processorCacheLogic, gpuCacheLogic, &outCV, blockIndex};

		err = clSetEventCallback(event, CL_COMPLETE, &cacheCallback, data);
		checkErrorCode(err, CL_SUCCESS, "clSetEventCallback");

		err = clEnqueueBarrierWithWaitList(processorCacheQueues[processorCacheIndex], 0, nullptr, &event);
	}

	// Get loop.
	while (1)
	{
		if (processorCacheLogic->readAny(indexSet, &processorCacheIndex, &blockIndex))
		{
			auto fromTo = getBlockBoundaries(blockIndex);
			return SignalBlock(processorCacheVertexArrays[processorCacheIndex], processorCacheGLBuffers[processorCacheIndex],
							   blockIndex, dataFile->getChannelCount(), fromTo.first, fromTo.second);
		}
		else
		{
			outCV.wait(processorCacheLock);
		}
	}
}

#undef fun

void SignalProcessor::dataFileCacheFiller(atomic<bool>* stop)
{
	unique_lock<recursive_mutex> lock(dataFileCacheMutex);

	while (stop->load() == false)
	{
		unsigned int cacheIndex;
		int blockIndex;

		bool notEmpty = dataFileCacheLogic->fill(&cacheIndex, &blockIndex);

		if (notEmpty)
		{
			auto fromTo = getBlockBoundaries(blockIndex);

			dataFile->readData(dataFileCache[cacheIndex], fromTo.first - offset + delay, fromTo.second + delay);

			dataFileCacheLogic->release(blockIndex);

			dataFileGpuCV.notify_one();
		}
		else
		{
			inCV.wait(lock);
		}
	}
}

void SignalProcessor::gpuCacheFiller(atomic<bool>* stop)
{
	unique_lock<recursive_mutex> gpuCacheLock(gpuCacheMutex);

	while (stop->load() == false)
	{
		unsigned int gpuCacheIndex;
		int blockIndex;

		bool notEmpty = gpuCacheLogic->fill(&gpuCacheIndex, &blockIndex);

		if (notEmpty)
		{
			gpuCacheLock.unlock();

			unsigned int dataFileCacheIndex;

			{
				unique_lock<recursive_mutex> dataFileCacheLock(dataFileCacheMutex);

				while (dataFileCacheLogic->read(blockIndex, &dataFileCacheIndex) == false)
				{
					dataFileGpuCV.wait(dataFileCacheLock);
				}
			}

			gpuCacheLock.lock();

			cl_int err;

			cl_event event = clCreateUserEvent(clContext->getCLContext(), &err);
			checkErrorCode(err, CL_SUCCESS, "clCreateUserEvent");

			cacheCallbackData* data = new cacheCallbackData {&gpuCacheMutex, &dataFileCacheMutex, dataFileCacheLogic, gpuCacheLogic, &gpuProcessorCV, blockIndex};

			err = clSetEventCallback(event, CL_COMPLETE, &cacheCallback, data);
			checkErrorCode(err, CL_SUCCESS, "clSetEventCallback");

			err = clEnqueueWriteBuffer(gpuCacheQueue, gpuCache[gpuCacheIndex], CL_FALSE, 0, dataFileGpuCacheBlockSize*sizeof(float), nullptr, 0, nullptr, &event);
			checkErrorCode(err, CL_SUCCESS, "clEnqueueWriteBuffer");
		}
		else
		{
			dataFileGpuCV.wait(gpuCacheLock);
		}
	}
}

void SignalProcessor::cacheCallback(cl_event event, cl_int event_command_exec_status, void* user_data)
{
	assert(event_command_exec_status == CL_COMPLETE);

	cacheCallbackData* data = reinterpret_cast<cacheCallbackData*>(user_data);

	lock_guard<recursive_mutex> lock1(*get<0>(*data));
	lock_guard<recursive_mutex> lock2(*get<1>(*data));

	int blockIndex = get<5>(*data);
	get<2>(*data)->release(blockIndex);
	get<3>(*data)->release(blockIndex);

	get<4>(*data)->notify_one();

	cl_int err = clReleaseEvent(event);
	checkErrorCode(err, CL_SUCCESS, "clReleaseEvent");

	delete data;
}
