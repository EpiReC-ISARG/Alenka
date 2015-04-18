#ifndef MONTAGEPROCESSOR_H
#define MONTAGEPROCESSOR_H

#include "montage.h"
#include "../openclprogram.h"

#include <CL/cl_gl.h>
#include <clFFT.h>

#include <vector>

class MontageProcessor
{
public:
	MontageProcessor(unsigned int offset, unsigned int blockWidth, int channelsInFile);
	~MontageProcessor();

	void change(Montage* montage);
	void process(cl_mem inBuffer, cl_mem outBuffer, cl_command_queue queue);
	unsigned int getNumberOfRows() const
	{
		return numberOfRows;
	}

private:
	cl_int inputRowLength;
	cl_int inputRowOffset;
	cl_int outputRowLength;
	cl_int channelsInFile;
	cl_kernel montageKernel = nullptr;
	unsigned int numberOfRows;

	void releaseMontage()
	{
		cl_int err;

		if (montageKernel != nullptr)
		{
			err = clReleaseKernel(montageKernel);
			checkClErrorCode(err, "clReleaseKernel()");
		}
	}
};

#endif // MONTAGEPROCESSOR_H
