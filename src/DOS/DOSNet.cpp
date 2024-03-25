//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "DOSNet.h"
#include "../HTTP.h"

#include <bios.h>
#include <io.h>
#include <fcntl.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "types.h"

#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"
#include "udp.h"
#include "dns.h"

// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.
volatile uint8_t CtrlBreakDetected = 0;
void __interrupt __far ctrlBreakHandler() {
	CtrlBreakDetected = 1;
}

void DOSNetworkDriver::Init()
{
	isConnected = false;

	//printf("Init network driver\n");
	if (Utils::parseEnv() != 0) {
		printf("Failed in parseEnv()\n");
		return;
	}

	//printf("Init network stack\n");
	if (Utils::initStack(MAX_CONCURRENT_HTTP_REQUESTS, TCP_SOCKET_RING_SIZE, ctrlBreakHandler, ctrlBreakHandler)) {
		printf("Failed to initialize TCP/IP\n");
		return;
	}

	for (int n = 0; n < MAX_CONCURRENT_HTTP_REQUESTS; n++)
	{
		requests[n] = new HTTPRequest();
		if (!requests[n])
		{
			Platform::FatalError("Could not allocate memory for HTTP request");
		}

		sockets[n] = new DOSTCPSocket();
		if (!sockets[n])
		{
			Platform::FatalError("Could not allocate memory for TCP socket");
		}
	}

	printf("Network interface initialised\n");
	isConnected = true;
}

void DOSNetworkDriver::Shutdown()
{
	if (isConnected)
	{
		Utils::endStack();
		isConnected = false;
	}
}

void DOSNetworkDriver::Update()
{
	if (isConnected)
	{
		PACKET_PROCESS_MULT(5);
		Arp::driveArp();
		Tcp::drivePackets();
		Dns::drivePendingQuery();

		for (int n = 0; n < MAX_CONCURRENT_HTTP_REQUESTS; n++)
		{
			requests[n]->Update();
		}
	}
}

HTTPRequest* DOSNetworkDriver::CreateRequest(char* url)
{
	if (isConnected)
	{
		for (int n = 0; n < MAX_CONCURRENT_HTTP_REQUESTS; n++)
		{
			if (requests[n]->GetStatus() == HTTPRequest::Stopped)
			{
				requests[n]->Open(url);
				return requests[n];
			}
		}
	}

	return NULL;
}

void DOSNetworkDriver::DestroyRequest(HTTPRequest* request)
{
	if (request)
	{
		request->Stop();
	}
}

// Returns zero on success, negative number is error
int DOSNetworkDriver::ResolveAddress(const char* name, NetworkAddress address, bool sendRequest)
{
	return Dns::resolve(name, address, sendRequest ? 1 : 0);
}

NetworkTCPSocket* DOSNetworkDriver::CreateSocket()
{
	for (int n = 0; n < MAX_CONCURRENT_HTTP_REQUESTS; n++)
	{
		if (sockets[n]->GetSock() == NULL)
		{
			TcpSocket* sock = TcpSocketMgr::getSocket();
			if (sock)
			{
				sockets[n]->SetSock(sock);
				return sockets[n];
			}
			return NULL;
		}
	}

	return NULL;
}

void DOSNetworkDriver::DestroySocket(NetworkTCPSocket* socket)
{
	socket->Close();
}

DOSTCPSocket::DOSTCPSocket()
{
	sock = NULL;
}

void DOSTCPSocket::SetSock(TcpSocket* inSock)
{
	sock = inSock;
	if (sock)
	{
		sock->rcvBuffer = recvBuffer;
		sock->rcvBufSize = TCP_RECV_BUFFER_SIZE;
	}
}

int DOSTCPSocket::Send(uint8_t* data, int length)
{
	if (!sock)
	{
		return -1;
	}
	return sock->send(data, length);
}

int DOSTCPSocket::Receive(uint8_t* buffer, int length)
{
	if (!sock)
	{
		return -1;
	}
	return sock->recv(buffer, length);
}

int DOSTCPSocket::Connect(NetworkAddress address, int port)
{
	uint16_t localport = 2048 + rand();
	return sock->connectNonBlocking(localport, address, port);
}

bool DOSTCPSocket::IsConnectComplete()
{
	return sock && sock->isConnectComplete();
}

bool DOSTCPSocket::IsClosed() 
{
	return !sock || sock->isClosed();
}

void DOSTCPSocket::Close()
{
	if (sock)
	{
		sock->closeNonblocking();
		TcpSocketMgr::freeSocket(sock);
		sock = NULL;
	}
}
