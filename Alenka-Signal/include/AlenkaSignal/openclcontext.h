#ifndef ALENKASIGNAL_OPENCLCONTEXT_H
#define ALENKASIGNAL_OPENCLCONTEXT_H

#include "openclprogram.h"

#ifdef __APPLE__
#include <OpenCL/cl_gl.h>
#else
#include <CL/cl_gl.h>
#endif

#include <memory>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Simplified error code test for OpenCL functions.
 * @param val_ The error code.
 */
#define checkClErrorCode(val_, message_)                                       \
  if ((val_) != CL_SUCCESS) {                                                  \
    std::stringstream ss;                                                      \
    ss << message_;                                                            \
    AlenkaSignal::OpenCLContext::CCEC(val_, ss.str(), __FILE__, __LINE__);     \
  }

namespace AlenkaSignal {

/**
 * @brief A wrapper for cl_context.
 *
 * CL_DEVICE_TYPE_ALL is used universally in the whole class (e.g. when calling
 * clGetDeviceIDs).
 */
class OpenCLContext {
  cl_context context;
  cl_platform_id platformId;
  cl_device_id deviceId;
  std::unique_ptr<OpenCLProgram> identityProgramFloat, identityProgramDouble,
      copyOnlyProgramFloat, copyOnlyProgramDouble;

public:
  /**
   * @brief OpenCLContext constructor.
   * @param platform Used as an index to an array returned by
   * clGetPlatformIDs().
   * @param device Used as an index to an array returned by clGetDeviceIDs().
   * @param shareCurrentGLContext If true, use current context during creation.
   *
   * The current OpenGL context is needed to setup proper communication between
   * OpenGL and OpenCL.
   * This is the only platform dependent code in the whole program and
   * will probably need to be modified when the code is ported to other
   * platforms.
   */
  OpenCLContext(unsigned int platform, unsigned int device,
                std::vector<cl_context_properties> properties =
                    std::vector<cl_context_properties>());
  ~OpenCLContext();

  /**
   * @brief Returns the underlying OpenCL object.
   */
  cl_context getCLContext() const { return context; }

  /**
   * @brief Returns the platform id resolved during construction.
   */
  cl_platform_id getCLPlatform() const { return platformId; }

  /**
   * @brief Returns the device id resolved during construction.
   */
  cl_device_id getCLDevice() const { return deviceId; }

  /**
   * @brief Returns a human-readable string with info about the selected
   * platform.
   *
   * clGetPlatformInfo() is used to retrieve this info.
   */
  std::string getPlatformInfo() const;

  /**
   * @brief Returns a human-readable string with info about the selected device.
   *
   * clGetDeviceInfo() is used to retrieve this info.
   */
  std::string getDeviceInfo() const;

  bool hasIdentityKernelFloat() const {
    return identityProgramFloat.get() != nullptr;
  }
  void setIdentityKernelFloat(std::unique_ptr<OpenCLProgram> val) {
    identityProgramFloat = std::move(val);
  }
  cl_kernel identityKernelFloat() const {
    return identityProgramFloat->createKernel("montage");
  }

  bool hasIdentityKernelDouble() const {
    return identityProgramDouble.get() != nullptr;
  }
  void setIdentityKernelDouble(std::unique_ptr<OpenCLProgram> val) {
    identityProgramDouble = std::move(val);
  }
  cl_kernel identityKernelDouble() const {
    return identityProgramDouble->createKernel("montage");
  }

  bool hasCopyOnlyKernelFloat() const {
    return copyOnlyProgramFloat.get() != nullptr;
  }
  void setCopyOnlyKernelFloat(std::unique_ptr<OpenCLProgram> val) {
    copyOnlyProgramFloat = std::move(val);
  }
  cl_kernel copyOnlyKernelFloat() const {
    return copyOnlyProgramFloat->createKernel("montage");
  }

  bool hasCopyOnlyKernelDouble() const {
    return copyOnlyProgramDouble.get() != nullptr;
  }
  void setCopyOnlyKernelDouble(std::unique_ptr<OpenCLProgram> val) {
    copyOnlyProgramDouble = std::move(val);
  }
  cl_kernel copyOnlyKernelDouble() const {
    return copyOnlyProgramDouble->createKernel("montage");
  }

  static void CCEC(cl_int val, std::string message, const char *file, int line);
  static std::string clErrorCodeToString(cl_int code);
  static void clfftInit();
  static void clfftDeinit();

  /**
   * @brief Prints the content of the data array to a file in a human-readable
   * format.
   *
   * Used for debugging purposes.
   */
  static void printBuffer(FILE *file, float *data, int n);

  /**
   * @brief Prints the content of the OpenCL buffer to a file in a
   * human-readable format.
   *
   * Used for debugging purposes.
   */
  static void printBuffer(FILE *file, cl_mem buffer, cl_command_queue queue);

  /**
   * @brief Prints the content of the data array to a new file with filePath in
   * a human-readable format.
   *
   * Used for debugging purposes.
   */
  static void printBuffer(const std::string &filePath, float *data, int n);

  /**
   * @brief Prints the content of the OpenCL buffer to a new file with filePath
   * in a human-readable format.
   *
   * Used for debugging purposes.
   */
  static void printBuffer(const std::string &filePath, cl_mem buffer,
                          cl_command_queue queue);

  static void printBufferDouble(FILE *file, double *data, int n);
  static void printBufferDouble(FILE *file, cl_mem buffer,
                                cl_command_queue queue);
  static void printBufferDouble(const std::string &filePath, double *data,
                                int n);
  static void printBufferDouble(const std::string &filePath, cl_mem buffer,
                                cl_command_queue queue);
};

} // namespace AlenkaSignal

#endif // ALENKASIGNAL_OPENCLCONTEXT_H
