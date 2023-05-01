/*
 * gVirtuS -- A GPGPU transparent virtualization component.
 *
 * Copyright (C) 2009-2010  The University of Napoli Parthenope at Naples.
 *
 * This file is part of gVirtuS.
 *
 * gVirtuS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gVirtuS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gVirtuS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by: Shreya Bhandare <shreyabhandare@vt.edu, shreyabhandare25@gmail.com>,
 *             Department of Computer Science, Virginia Tech
 */

/**
 * @file   RdmaCommunicator.cpp
 * @author Shreya Bhandare <shreyabhandare@vt.edu, shreyabhandare25@gmail.com>
 *
 * @brief
 *
 *
 */
//#define DEBUG

#include "RdmaCommunicator.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <WinSock2.h>
static bool initialized = false;
#endif

#include <gvirtus/communicators/Endpoint.h>
#include <gvirtus/communicators/Endpoint_Rdma.h>
#include <cstdlib>
#include <cstring>
#include <iostream>


using namespace std;
using gvirtus::communicators::RdmaCommunicator;

RdmaCommunicator::RdmaCommunicator(const std::string &communicator) {
// const char *valueptr = strstr(communicator.c_str(), "://") + 3;
// const char *portptr = strchr(valueptr, ':');
// if (portptr == NULL) throw "Port not specified.";

// short port = (short)strtol(portptr + 1, NULL, 10);
// mPortNo = new char[sizeof(short)];
// memcpy(mPortNo, &port, sizeof(short));

// char *hostname = strdup(valueptr);
// hostname[portptr - valueptr] = 0;
// std:string mHostname = string(hostname);

// struct hostent *ent = gethostbyname(hostname);
// free(hostname);
// if (ent == NULL){
// 	throw "TcpCommunicator: Can't resolve hostname '" + mHostname + "'.";
// }

// int mInAddrSize = ent->h_length;
// mInAddr = new char[mInAddrSize];
// memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
mPortNo = "8888";
memset(&hints, 0, sizeof hints);
hints.ai_port_space = RDMA_PS_TCP;
}

RdmaCommunicator::RdmaCommunicator(const char *hostname, short port) {
    std::string mHostname = string(hostname);
    struct hostent *ent = gethostbyname(hostname);
    if (ent == NULL)
        throw "RdmaCommunicator: Can't resolve hostname '" + mHostname + "'.";
    int mInAddrSize = ent->h_length;
    mInAddr = new char[mInAddrSize];
    memcpy(mInAddr, *ent->h_addr_list, mInAddrSize);
    mPortNo = new char[sizeof(short)];
    memcpy(mPortNo, &port, sizeof(short));
    
    memset(&hints, 0, sizeof hints);
	hints.ai_port_space = RDMA_PS_TCP;
}

RdmaCommunicator::~RdmaCommunicator() {
    delete[] mInAddr;
    delete[] mPortNo;
}

void RdmaCommunicator::Serve() {

    hints.ai_flags = RAI_PASSIVE;
    
    const char *serverip = "192.168.0.11";
    const char *portnum = "8000"; 
    auto res = rdma::addrinfo::get(serverip, portnum, &hints);

	memset(&init_attr, 0, sizeof init_attr);

	ibv::queuepair::Capabilities cap;
	cap.setMaxSendWr(1);
	cap.setMaxRecvWr(1);
	cap.setMaxSendSge(1);
	cap.setMaxRecvSge(1);
	cap.setMaxInlineData(16);

	init_attr.setCapabilities(cap);
	init_attr.setSignalAll(1);
	listen_id = rdma::createEP(res, boost::none, boost::make_optional(init_attr));
	listen_id->listen(0);
 
}

const gvirtus::communicators::Communicator *const RdmaCommunicator::Accept()
    const {
	id = listen_id->getRequest();
	ibv::queuepair::Attributes qp_attr;
	memset(&qp_attr, 0, sizeof qp_attr);
	memset(&init_attr, 0, sizeof init_attr);

	id->getQP()->query(qp_attr, {ibv::queuepair::AttrMask::CAP},  init_attr, {});
	if (init_attr.getCapabilities().getMaxInlineData() >= 16)
		inlineFlag = true;
	else
		printf("rdma_server: device doesn't support IBV_SEND_INLINE, "
		       "using sge sends\n");
	//qp = id->getQP();
	id->accept(nullptr);
	//return communicator
	return nullptr;
}

void RdmaCommunicator::Connect() {
    ibv::queuepair::InitAttributes attr;
    const char *serverip = "192.168.0.27";
    const char *portnum = "8000"; 
    auto res = rdma::addrinfo::get(serverip, portnum, &hints);
    memset(&attr, 0, sizeof(attr));

    ibv::queuepair::Capabilities cap;
    cap.setMaxSendWr(1);
	cap.setMaxRecvWr(1);
	cap.setMaxSendSge(1);
	cap.setMaxRecvSge(1);
	cap.setMaxInlineData(16);

	attr.setCapabilities(cap);
	attr.setSignalAll(1);

    id = rdma::createEP(res, boost::none, boost::make_optional(attr));
	// Check to see if we got inline data allowed or not
	if (attr.getCapabilities().getMaxInlineData() >= 16)
		inlineFlag = true;
	else
		printf("rdma_client: device doesn't support IBV_SEND_INLINE, using sge sends\n");

	auto temp = id->getQP();
	
	id->connect(nullptr);
  
}

void RdmaCommunicator::Close() {
	id->disconnect();
}

size_t RdmaCommunicator::Read(char *buffer, size_t size) {
	auto recv_mr = id->getPD()->registerMemoryRegion(buffer, size,
						    { ibv::AccessFlag::LOCAL_WRITE });
    auto recv_wr = ibv::workrequest::Simple<ibv::workrequest::Recv>();
	recv_wr.setLocalAddress(recv_mr->getSlice());
	ibv::workrequest::Recv *bad_recv_wr;
	id->getQP()->postRecv(recv_wr, bad_recv_wr);
	
	auto recv_cq = id->getQP()->getRecvCQ();
	while ((recv_cq->poll(1, &wc)) == 0);
	return size;
}

size_t RdmaCommunicator::Write(const char *cbuffer, size_t size) {
	char* buffer = new char[size];
	memcpy(buffer, cbuffer, size);
	auto send_mr = id->getPD()->registerMemoryRegion(buffer, size, {});
	auto send_wr = ibv::workrequest::Simple<ibv::workrequest::Send>();
	send_wr.setLocalAddress(send_mr->getSlice());
	ibv::workrequest::SendWr *bad_wr;
	if (inlineFlag) {
		send_wr.setFlags({ ibv::workrequest::Flags::INLINE });
	}
	id->getQP()->postSend(send_wr, bad_wr);

	auto send_cq = id->getQP()->getSendCQ();
	while ((send_cq->poll(1, &wc)) == 0);
	return size;
}

void RdmaCommunicator::Sync() {  }


//
extern "C" std::shared_ptr<RdmaCommunicator> create_communicator(
    std::shared_ptr<gvirtus::communicators::Endpoint> end) {
  std::string arg =
      "Rdma://" +
      std::dynamic_pointer_cast<gvirtus::communicators::Endpoint_Rdma>(end)
          ->address() +
      ":" +
      std::to_string(
          std::dynamic_pointer_cast<gvirtus::communicators::Endpoint_Rdma>(end)
              ->port());
  return std::make_shared<RdmaCommunicator>(arg);
}
