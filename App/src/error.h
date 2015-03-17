#ifndef ERROR_H
#define ERROR_H

#include "options.h"

#include <CL/cl_gl.h>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cassert>
#include <iostream>

namespace
{
template <typename T>
void printEC(T val, std::stringstream& ss)
{
	ss << std::dec << val << "(0x" << std::hex << val << std::dec << ")";
}

template <typename T>
void CEC(T val, T expected, std::string message, const char* file, int line)
{
	std::stringstream ss;

	ss << "Unexpected error code: ";
	printEC(val, ss);
	ss << ", required ";
	printEC(expected, ss);
	ss << ". ";

	ss << message << " " << file << ":" << line;

	throw std::runtime_error(ss.str());
}

template <typename T>
void CNEC(T val, std::string message, const char* file, int line)
{
	std::stringstream ss;

	ss << "Error code returned ";
	printEC(val, ss);
	ss << ". ";

	ss << message << " " << file << ":" << line;

	throw std::runtime_error(ss.str());
}
}

#define checkErrorCode(val_, expected_, message_) if((val_) != (expected_)) { std::stringstream ss; ss << message_; CEC(val_, expected_, ss.str(), __FILE__, __LINE__); }
#define checkNotErrorCode(val_, expected_, message_) if((val_) == (expected_)) { std::stringstream ss; ss << message_; CNEC(val_, ss.str(), __FILE__, __LINE__); }

inline std::size_t freadChecked(void* data, std::size_t size, std::size_t n, FILE* file)
{
	using namespace std;

	size_t elementsRead = fread(data, size, n, file);
	if (elementsRead != n)
	{
		stringstream ss;
		if (feof(file))
		{
			ss << "EOF reached prematurely.";
		}
		else
		{
			assert(ferror(file));
			ss << "Error while reading data from file.";
		}
		throw runtime_error(ss.str());
	}
	return elementsRead;
}

inline void printBuffer(FILE* file, float* data, int n)
{
#ifndef NDEBUG
	if (PROGRAM_OPTIONS.isSet("printBuffers"))
	{
		for (int i = 0; i < n; ++i)
		{
			fprintf(file, "%f\n", data[i]);
		}
	}
#else
	(void)file; (void)data; (void)n;
#endif
}

inline void printBuffer(FILE* file, cl_mem buffer, cl_command_queue queue)
{
#ifndef NDEBUG
	if (PROGRAM_OPTIONS.isSet("printBuffers"))
	{
		cl_int err;

		size_t size;
		err = clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof(size_t), &size, nullptr);
		checkErrorCode(err, CL_SUCCESS, "clGetMemObjectInfo");

		float* tmp = new float[size];

		err = clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, size, tmp, 0, nullptr, nullptr);
		checkErrorCode(err, CL_SUCCESS, "clGetMemObjectInfo");

		printBuffer(file, tmp, size/sizeof(float));

		delete[] tmp;
	}
#else
	(void)file; (void)buffer; (void)queue;
#endif
}

inline void printBuffer(const std::string& filePath, float* data, int n)
{
#ifndef NDEBUG
	using namespace std;

	if (PROGRAM_OPTIONS.isSet("printBuffers"))
	{
		string path = PROGRAM_OPTIONS["printBuffersFolder"].as<string>() + "/" + filePath;

		FILE* file = fopen(path.c_str(), "w");
		checkNotErrorCode(file, nullptr, "File '" << path << "' could not be open for writing.");

		printBuffer(file, data, n);

		fclose(file);
	}
#else
	(void)filePath; (void)data; (void)n;
#endif
}

inline void printBuffer(const std::string& filePath, cl_mem buffer, cl_command_queue queue)
{
#ifndef NDEBUG
	using namespace std;

	if (PROGRAM_OPTIONS.isSet("printBuffers"))
	{
		string path = PROGRAM_OPTIONS["printBuffersFolder"].as<string>() + "/" + filePath;

		FILE* file = fopen(path.c_str(), "w");
		checkNotErrorCode(file, nullptr, "File '" << path << "' could not be open for writing.");

		printBuffer(file, buffer, queue);

		fclose(file);
	}
#else
	(void)filePath; (void)buffer; (void)queue;
#endif
}

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(GLOBAL_LOGGER, boost::log::sources::logger_mt)
#define logToFile(a_) BOOST_LOG_FUNCTION(); BOOST_LOG(GLOBAL_LOGGER::get()) << a_
#define logToBoth(a_) logToFile(a_); std::cerr << a_ << " [in " << __FILE__ << ":" << __LINE__ << "]"

#endif // ERROR_H
