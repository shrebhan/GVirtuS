

/**
 * @file   RdmaCommunicator.h
 * @author Shreya Bhandare <shreyabhandare25@gmail.com>
 * @date   
 *
 * @brief
 *
 *
 */

#pragma once

#ifdef _WIN32
#include <fstream>
#else
#include <ext/stdio_filebuf.h>
#endif

#include "gvirtus/communicators/Communicator.h"

namespace gvirtus::communicators {
/**
 * RdmaCommunicator implements a Communicator for RDMA transfer
 */
class RdmaCommunicator : public Communicator {
 public:
  RdmaCommunicator() = default;
  RdmaCommunicator(const std::string &communicator);
  RdmaCommunicator(const char *hostname, short port);
  RdmaCommunicator(int fd, const char *hostname);
  virtual ~RdmaCommunicator();
  void Serve();
  const Communicator *const Accept() const;
  void Connect();
  size_t Read(char *buffer, size_t size);
  size_t Write(const char *buffer, size_t size);
  void Sync();
  void Close();

  std::string to_string() override { return "Rdmacommunicator"; }

 private:
  void InitializeStream();
  std::istream *mpInput;
  std::ostream *mpOutput;
  std::string mHostname;
  char *mInAddr;
  int mInAddrSize;
  short mPort;
  int mSocketFd;
#ifdef _WIN32
  std::filebuf *mpInputBuf;
  std::filebuf *mpOutputBuf;
#else
  __gnu_cxx::stdio_filebuf<char> *mpInputBuf;
  __gnu_cxx::stdio_filebuf<char> *mpOutputBuf;
#endif
};
}  // namespace gvirtus::communicators
