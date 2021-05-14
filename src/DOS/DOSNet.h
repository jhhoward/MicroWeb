#pragma once

#include <stdint.h>
#include "../Platform.h"

#define MAX_CONCURRENT_HTTP_REQUESTS 3
#define HOSTNAME_LEN        (80)
#define PATH_LEN           (256)

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
