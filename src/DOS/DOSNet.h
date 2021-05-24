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

#pragma once

#include <stdint.h>
#include "../Platform.h"
#include "../URL.h"

#define TCP_RECV_BUFFER_SIZE  (16384)
#define MAX_CONCURRENT_HTTP_REQUESTS 3
#define HOSTNAME_LEN        (80)
#define PATH_LEN           (MAX_URL_LENGTH)

typedef uint8_t  IpAddr_t[4];   // An IPv4 address is 4 bytes
struct TcpSocket;

class DOSHTTPRequest : public HTTPRequest
{
public:
	DOSHTTPRequest();

	void Open(char* url);

	virtual HTTPRequest::Status GetStatus() { return status; }
	virtual size_t ReadData(char* buffer, size_t count);
	virtual void Stop();
	void Update();
	virtual const char* GetStatusString();

private:
	enum InternalStatus
	{
		// Errors
		InvalidPort,
		InvalidProtocol,
		SocketCreationError,
		SocketConnectionError,
		HeaderSendError,
		ContentReceiveError,

		// Connection states
		QueuedDNSRequest,
		WaitingDNSResolve,
		OpeningSocket,
		ConnectingSocket,
		SendHeaders,
		ReceiveHeaders,
	};

	void Error(InternalStatus statusError);

	HTTPRequest::Status status;
	InternalStatus internalStatus;

	char hostname[HOSTNAME_LEN];
	char path[PATH_LEN];
	IpAddr_t hostAddr;
	uint16_t serverPort;
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

	virtual HTTPRequest* CreateRequest(char* url);
	virtual void DestroyRequest(HTTPRequest* request);

private:
	DOSHTTPRequest requests[MAX_CONCURRENT_HTTP_REQUESTS];
	bool isConnected;
};
