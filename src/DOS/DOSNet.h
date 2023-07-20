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

#ifndef _DOSNET_H_
#define _DOSNET_H_

#include <stdint.h>
#include "../Platform.h"
#include "../URL.h"

#ifdef HP95LX
#define TCP_RECV_BUFFER_SIZE  (8192)
#define MAX_CONCURRENT_HTTP_REQUESTS 1
#else
#define TCP_RECV_BUFFER_SIZE  (16384)
#define MAX_CONCURRENT_HTTP_REQUESTS 3
#endif

struct TcpSocket;

class DOSTCPSocket : public NetworkTCPSocket
{
public:
	DOSTCPSocket();
	TcpSocket* GetSock() { return sock; }
	void SetSock(TcpSocket* sock);

	virtual int Send(uint8_t* data, int length) override;
	virtual int Receive(uint8_t* buffer, int length) override;
	virtual int Connect(NetworkAddress address, int port) override;
	virtual bool IsConnectComplete() override;
	virtual bool IsClosed() override;
	virtual void Close() override;

private:
	TcpSocket* sock;
	uint8_t recvBuffer[TCP_RECV_BUFFER_SIZE];
};

class DOSNetworkDriver : public NetworkDriver
{
public:
	virtual void Init();
	virtual void Shutdown();
	virtual void Update();

	virtual bool IsConnected() { return isConnected; }

	// Returns zero on success, negative number is error
	virtual int ResolveAddress(const char* name, NetworkAddress address, bool sendRequest) override;

	virtual HTTPRequest* CreateRequest(char* url);
	virtual void DestroyRequest(HTTPRequest* request);

	virtual NetworkTCPSocket* CreateSocket() override;
	virtual void DestroySocket(NetworkTCPSocket* socket) override;

private:
	HTTPRequest* requests[MAX_CONCURRENT_HTTP_REQUESTS];
	DOSTCPSocket sockets[MAX_CONCURRENT_HTTP_REQUESTS];

	bool isConnected;
};

#endif
