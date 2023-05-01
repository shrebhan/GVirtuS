

/**
 * @file   RdmaCommunicator.h
 * @author Shreya Bhandare <shreyabhandare@vt.edu, shreyabhandare25@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <librdmacmcpp.h>

namespace gvirtus::communicators {
/**
 * RdmaCommunicator implements a Communicator for RDMA transfer
 */
class RdmaCommunicator : public Communicator {
 public:
  RdmaCommunicator() = default;
  RdmaCommunicator(const std::string &communicator);
  RdmaCommunicator(const char *hostname, short port);
  //RdmaCommunicator(int fd, const char *hostname);

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

  char *mInAddr;
  char *mPortNo;

  //rdma structures 
  struct rdma_addrinfo hints;

	ibv::workcompletion::WorkCompletion wc;
	mutable bool inlineFlag = false;
  //std::unique_ptr<ibv::memoryregion::MemoryRegion> send_mr; 
  //std::unique_ptr<ibv::memoryregion::MemoryRegion> recv_mr;
  mutable std::unique_ptr<rdma::ID> id;
  ibv::queuepair::QueuePair* qp;
  std::unique_ptr<rdma::ID> listen_id;
  mutable ibv::queuepair::InitAttributes init_attr;;


};
}  // namespace gvirtus::communicators
