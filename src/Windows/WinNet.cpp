#include <WinSock2.h>
#include <WS2tcpip.h>
#include "WinNet.h"
#include "../HTTP.h"

#pragma comment(lib, "ws2_32.lib")

WindowsNetworkDriver::WindowsNetworkDriver() :isConnected(false)
{

}

void WindowsNetworkDriver::Init()
{
	WSADATA wsData;
	if (WSAStartup(MAKEWORD(2, 2), &wsData) == 0) {
		isConnected = true;
	}

	for (int n = 0; n < MAX_CONCURRENT_REQUESTS; n++)
	{
		requests[n] = NULL;
	}
}

void WindowsNetworkDriver::Shutdown()
{
	WSACleanup();
}

int WindowsNetworkDriver::ResolveAddress(const char* name, NetworkAddress address, bool sendRequest)
{
	// Set up the address hints for the host lookup
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;     // Allow IPv4
	hints.ai_socktype = SOCK_STREAM; // Stream socket (TCP)
	hints.ai_flags = AI_PASSIVE;     // For wildcard IP address

	// Asynchronous hostname lookup with non-blocking sockets
	struct addrinfo* result = nullptr;
	int getAddrResult = getaddrinfo(name, NULL, &hints, &result);
	if (getAddrResult != 0) {
		// Error doing lookup
		return -1;
	}

	// Loop through the address results to get IPv4 addresses
	for (struct addrinfo* addr = result; addr != NULL; addr = addr->ai_next) {
		if (addr->ai_family == AF_INET) { // IPv4

			struct sockaddr_in* ipv4 = reinterpret_cast<struct sockaddr_in*>(addr->ai_addr);

			address[0] = (ipv4->sin_addr.s_addr >> 0) & 0xFF;
			address[1] = (ipv4->sin_addr.s_addr >> 8) & 0xFF;
			address[2] = (ipv4->sin_addr.s_addr >> 16) & 0xFF;
			address[3] = (ipv4->sin_addr.s_addr >> 24) & 0xFF;

			return 0;
		}
	}

	// Free the memory allocated for the address info
	freeaddrinfo(result);
	return 1;
}

NetworkTCPSocket* WindowsNetworkDriver::CreateSocket()
{
	return new WindowsTCPSocket();
}

void WindowsNetworkDriver::DestroySocket(NetworkTCPSocket* socket)
{
	if (socket)
	{
		socket->Close();
	}
	delete socket;
}

HTTPRequest* WindowsNetworkDriver::CreateRequest()
{
	for (int n = 0; n < MAX_CONCURRENT_REQUESTS; n++)
	{
		if (requests[n] == NULL)
		{
			requests[n] = new HTTPRequest();
			return requests[n];
		}
	}

	return NULL;
}

void WindowsNetworkDriver::DestroyRequest(HTTPRequest* request)
{
	for (int n = 0; n < MAX_CONCURRENT_REQUESTS; n++)
	{
		if (requests[n] == request)
		{
			delete request;
			requests[n] = NULL;
		}
	}
}

void WindowsNetworkDriver::Update()
{
	for (int n = 0; n < MAX_CONCURRENT_REQUESTS; n++)
	{
		if (requests[n])
		{
			requests[n]->Update();
		}
	}
}

WindowsTCPSocket::WindowsTCPSocket()
{
	// Create a socket
	sock = socket(AF_INET, SOCK_STREAM, 0);

	// Set the socket to non-blocking mode
	u_long mode = 1;
	if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
		Close();
	}
}

int WindowsTCPSocket::Send(uint8_t* data, int length)
{
	if (sock != INVALID_SOCKET)
	{
		// Wait for the socket to be writable (connected) with a timeout
		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(sock, &writeSet);

		timeval timeout;
		timeout.tv_sec = 0; // 0 seconds timeout
		timeout.tv_usec = 0;

		int result = select(0, nullptr, &writeSet, nullptr, &timeout);
		if (result < 0) {
			// Failed to connect to the server
			Close();
			return -1;
		}

		if (result > 0)
		{
			// Send data to the server
			result = send(sock, (const char*)data, length, 0);
			if (result == SOCKET_ERROR) {
				// Failed to send data to the server
				Close();
				return -1;
			}
		}

		return result;
	}
	else
	{
		return -1;
	}
}

int WindowsTCPSocket::Receive(uint8_t* buffer, int length)
{
	// Wait for the socket to be readable (response received) with a timeout
	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(sock, &readSet);

	timeval timeout;
	timeout.tv_sec = 0; // 0 seconds timeout
	timeout.tv_usec = 0;

	int result = select(0, &readSet, nullptr, nullptr, &timeout);
	if (result < 0) {
		// Failed to receive data
		Close();
		return -1;
	}
	else if (result > 0)
	{
		// Receive data from the server
		result = recv(sock, (char*)buffer, length, 0);
		if (result == SOCKET_ERROR) {
			// Failed to receive data
			Close();
			return -1;
		}
	}

	return result;
}

int WindowsTCPSocket::Connect(NetworkAddress address, int port)
{
	// Connect to the server
	struct sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port); // Change this to the server's port number
	serverAddr.sin_addr.S_un.S_addr = *(reinterpret_cast<uint32_t*>(address));

	int result = connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		return -1;
	}

	return 0;
}

bool WindowsTCPSocket::IsConnectComplete()
{
	return sock != INVALID_SOCKET;
}

bool WindowsTCPSocket::IsClosed()
{
	return sock == INVALID_SOCKET;
}

void WindowsTCPSocket::Close()
{
	if (sock != INVALID_SOCKET)
	{
		closesocket(sock);
		sock = INVALID_SOCKET;
	}
}

