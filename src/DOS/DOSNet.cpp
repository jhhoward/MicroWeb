#include "DOSNet.h"

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

#define TCP_RECV_BUFFER  (16384)

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
		fprintf(stderr, "\nFailed in parseEnv()\n");
		//exit(1);
		return;
	}

	//printf("Init network stack\n");
	if (Utils::initStack(MAX_CONCURRENT_HTTP_REQUESTS, TCP_SOCKET_RING_SIZE, ctrlBreakHandler, ctrlBreakHandler)) {
		fprintf(stderr, "\nFailed to initialize TCP/IP - exiting\n");
		//exit(1);
		return;
	}

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
			requests[n].Update();
		}
	}
}

HTTPRequest* DOSNetworkDriver::CreateRequest(char* url)
{
	if (isConnected)
	{
		for (int n = 0; n < MAX_CONCURRENT_HTTP_REQUESTS; n++)
		{
			if (requests[n].GetStatus() == HTTPRequest::Stopped)
			{
				requests[n].Open(url);
				return &requests[n];
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

// sock_printf
//
// This will loop until it can push all of the data out.
// Does not check the incoming data length, so don't flood it.
// (The extra data will be ignored/truncated ...)
//
// Returns 0 on success, -1 on error

#define SOCK_PRINTF_SIZE  (1024)
static char spb[SOCK_PRINTF_SIZE];

static int sock_printf(TcpSocket* sock, char* fmt, ...) {

	va_list ap;
	va_start(ap, fmt);
	int vsrc = vsnprintf(spb, SOCK_PRINTF_SIZE, fmt, ap);
	va_end(ap);

	if ((vsrc < 0) || (vsrc >= SOCK_PRINTF_SIZE)) {
		//errorMessage("Formatting error in sock_printf\n");
		return -1;
	}

	uint16_t bytesToSend = vsrc;
	uint16_t bytesSent = 0;

	while (bytesSent < bytesToSend) {

		// Process packets here in case we have tied up the outgoing buffers.
		// This will give us a chance to push them out and free them up.

		PACKET_PROCESS_MULT(5);
		Arp::driveArp();
		Tcp::drivePackets();

		int rc = sock->send((uint8_t*)(spb + bytesSent), bytesToSend - bytesSent);
		if (rc > 0) {
			bytesSent += rc;
		}
		else if (rc == 0) {
			// Out of send buffers maybe?  Loop around to process packets
		}
		else {
			return -1;
		}

	}

	return 0;
}


DOSHTTPRequest::DOSHTTPRequest() : status(HTTPRequest::Stopped), sock(NULL)
{
}

void DOSHTTPRequest::Open(char* url)
{
	if (strnicmp(url, "http://", 7) == 0) {

		char* hostnameStart = url + 7;

		// Scan ahead for another slash; if there is none then we
		// only have a server name and we should fetch the top
		// level directory.

		char* proxy = getenv("HTTP_PROXY");
		if (proxy == NULL) {

			char* pathStart = strchr(hostnameStart, '/');
			if (pathStart == NULL) {

				strncpy(hostname, hostnameStart, HOSTNAME_LEN);
				hostname[HOSTNAME_LEN - 1] = 0;

				path[0] = '/';
				path[1] = 0;

			}
			else {

				strncpy(hostname, hostnameStart, pathStart - hostnameStart);
				hostname[HOSTNAME_LEN - 1] = 0;

				strncpy(path, pathStart, PATH_LEN);
				path[PATH_LEN - 1] = 0;

			}

		}
		else {

			strncpy(hostname, proxy, HOSTNAME_LEN);
			hostname[HOSTNAME_LEN - 1] = 0;

			strncpy(path, url, PATH_LEN);
			path[PATH_LEN - 1] = 0;

		}

		serverPort = 80;
		char* portStart = strchr(hostname, ':');

		if (portStart != NULL) {
			serverPort = atoi(portStart + 1);
			if (serverPort == 0) {
				Error(InvalidPort);
				return;
			}

			// Truncate hostname early
			*portStart = 0;
		}

		status = HTTPRequest::Connecting;
		internalStatus = QueuedDNSRequest;
	}
	else {
		// Need to specify a URL starting with http://
		Error(InvalidProtocol);
	}
}

size_t DOSHTTPRequest::ReadData(char* buffer, size_t count)
{
	if (status == HTTPRequest::Downloading && sock)
	{
		int16_t rc = sock->recv((unsigned char*) buffer, count);
		if (rc < 0)
		{
			Error(ContentReceiveError);
		}
		else
		{
			return (size_t)(rc);
		}
	}
	return 0;
}

void DOSHTTPRequest::Stop()
{
	if (sock)
	{
		sock->closeNonblocking();
		TcpSocketMgr::freeSocket(sock);
		sock = NULL;
	}
	status = HTTPRequest::Stopped;
}

void DOSHTTPRequest::Error(InternalStatus statusError)
{
	status = HTTPRequest::Error;
	internalStatus = statusError;
}

void DOSHTTPRequest::Update()
{
	switch (status)
	{
		case HTTPRequest::Connecting:
		{
			switch (internalStatus)
			{
			case QueuedDNSRequest:
				{
					int8_t rc = Dns::resolve(hostname, hostAddr, 1);
					if (rc == 1)
					{
						internalStatus = WaitingDNSResolve;
					}
					else if (rc == 0)
					{
						internalStatus = OpeningSocket;

						//printf("Host %s resolved to %d.%d.%d.%d\n", hostname, hostAddr[0], hostAddr[1], hostAddr[2], hostAddr[3]);
						//status = HTTPRequest::Stopped;
					}
				}
				break;
			case WaitingDNSResolve:
				{
					int8_t rc = Dns::resolve(hostname, hostAddr, 0);
					if (rc == 0)
					{
						internalStatus = OpeningSocket;
						//printf("Host %s resolved to %d.%d.%d.%d\n", hostname, hostAddr[0], hostAddr[1], hostAddr[2], hostAddr[3]);
						//status = HTTPRequest::Stopped;
					}
				}
				break;
			case OpeningSocket:
				{
					sock = TcpSocketMgr::getSocket();
					if (sock->setRecvBuffer(TCP_RECV_BUFFER)) 
					{
						Error(SocketCreationError);
						break;
					}

					uint16_t localport = 2048 + rand();

					if (sock->connectNonBlocking(localport, hostAddr, serverPort))
					{
						Error(SocketCreationError);
						break;
					}
					internalStatus = ConnectingSocket;
				}
				break;
			case ConnectingSocket:
				{
					if (sock->isConnectComplete()) 
					{
						internalStatus = SendHeaders;
						break;
					}
					else if(sock->isClosed())
					{
						Error(SocketConnectionError);
						break;
					}
				}
				break;
			case SendHeaders:
				{
					if(sock_printf(sock, "GET %s\r\n", path))
					{
						Error(HeaderSendError);
					}
					else
					{
						status = HTTPRequest::Downloading;
					}
				}
				break;
			}
		}
		break;

		case HTTPRequest::Downloading:
		{
			if (sock->isRemoteClosed())
			{
				//status = HTTPRequest::Finished;
			}
		}
		break;
	}
}
