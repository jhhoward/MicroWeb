#ifndef WINNET_H_
#define WINNET_H_

#include <WinSock2.h>
#include "../Platform.h"

#define MAX_CONCURRENT_REQUESTS 2

class WindowsNetworkDriver : public NetworkDriver
{
public:
	WindowsNetworkDriver();

	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void Update() override;

	virtual bool IsConnected() override { return isConnected; }

	// Returns zero on success, negative number is error
	virtual int ResolveAddress(const char* name, NetworkAddress address, bool sendRequest) override;

	virtual NetworkTCPSocket* CreateSocket() override;
	virtual void DestroySocket(NetworkTCPSocket* socket) override;

	virtual HTTPRequest* CreateRequest() override;
	virtual void DestroyRequest(HTTPRequest* request) override;

private:
	HTTPRequest* requests[MAX_CONCURRENT_REQUESTS];

	bool isConnected;
};

class WindowsTCPSocket : public NetworkTCPSocket
{
public:
	WindowsTCPSocket();
	virtual int Send(uint8_t* data, int length) override;
	virtual int Receive(uint8_t* buffer, int length) override;
	virtual int Connect(NetworkAddress address, int port) override;
	virtual bool IsConnectComplete() override;
	virtual bool IsClosed() override;
	virtual void Close() override;

private:
	SOCKET sock;
};

#endif

