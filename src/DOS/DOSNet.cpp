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

DOSHTTPRequest::DOSHTTPRequest() : status(HTTPRequest::Stopped), sock(NULL)
{
}

void DOSHTTPRequest::Reset()
{
	lineBufferSize = 0;
	lineBufferSendPos = -1;
}

void DOSHTTPRequest::WriteLine(char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int maxWriteSize = LINE_BUFFER_SIZE - lineBufferSize - 2;
	int vsrc = vsnprintf(lineBuffer + lineBufferSize, maxWriteSize, fmt, ap);
	va_end(ap);

	if ((vsrc < 0) || (vsrc >= maxWriteSize)) 
	{
		Error(WriteLineError);
		lineBufferSendPos = -1;
	}
	else
	{
		if (lineBufferSendPos == -1)
		{
			lineBufferSendPos = 0;
		}
		lineBufferSize += vsrc;
		lineBuffer[lineBufferSize++] = '\r';
		lineBuffer[lineBufferSize++] = '\n';
	}
}

bool DOSHTTPRequest::SendPendingWrites()
{
	if (sock)
	{
		if (lineBufferSendPos < 0)
		{
			return false;
		}

		int rc = sock->send((uint8_t*)(lineBuffer + lineBufferSendPos), lineBufferSize - lineBufferSendPos);
		if (rc > 0) 
		{
			lineBufferSendPos += rc;
			if (lineBufferSendPos >= lineBufferSize)
			{
				lineBufferSendPos = -1;
				lineBufferSize = 0;
			}
		}
		else if (rc < 0) 
		{
			Error(WriteLineError);
			lineBufferSendPos = -1;
		}
		return lineBufferSendPos >= 0;
	}
	return false;
}

void DOSHTTPRequest::Open(char* inURL)
{
	url = inURL;
	Reset();

	if (strnicmp(url.url, "http://", 7) == 0) {

		char* hostnameStart = url.url + 7;

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
				hostname[pathStart - hostnameStart] = '\0';
				hostname[HOSTNAME_LEN - 1] = 0;

				strncpy(path, pathStart, PATH_LEN);
				path[PATH_LEN - 1] = 0;

			}

		}
		else {

			strncpy(hostname, proxy, HOSTNAME_LEN);
			hostname[HOSTNAME_LEN - 1] = 0;

			strncpy(path, url.url, PATH_LEN);
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
	else if (strnicmp(url.url, "https://", 8) == 0) {
		status = HTTPRequest::UnsupportedHTTPS;
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
	if(sock)
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
	if (SendPendingWrites())
	{
		return;
	}

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
					else
					{
						//printf("Resolve host \"%s\"", hostname);
						//getchar();
					}
				}
				break;
			case OpeningSocket:
				{
					sock = TcpSocketMgr::getSocket();
					if (!sock)
					{
						Error(SocketCreationError);
					}
					sock->rcvBuffer = recvBuffer;
					sock->rcvBufSize = TCP_RECV_BUFFER_SIZE;

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
					WriteLine("GET %s HTTP/1.0", path);
					WriteLine("User-Agent: MicroWeb " __DATE__);
					WriteLine("Host: %s", hostname);
					WriteLine("Connection: close");
					WriteLine();
					internalStatus = ReceiveHeaderResponse;
				}
				break;
			case ReceiveHeaderResponse:
				{
					if (ReadLine())
					{
						if ((strncmp(lineBuffer, "HTTP/1.0", 8) != 0) && (strncmp(lineBuffer, "HTTP/1.1", 8) != 0)) {
							Error(UnsupportedHTTPError);
							return;
						}

						// Skip past HTTP version number
						char* s = lineBuffer + 8;
						char* s2 = s;

						// Skip past whitespace
						while (*s) {
							if (*s != ' ' && *s != '\t') break;
							s++;
						}

						if ((s == s2) || (*s == 0) || (sscanf(s, "%3d", &responseCode) != 1)) {
							Error(MalformedHTTPVersionLineError);
							return;
						}

						//printf("Response code: %d", responseCode);
						//getchar();
						internalStatus = ReceiveHeaderContent;
					}
				}
				break;
			case ReceiveHeaderContent:
				{
					if (ReadLine())
					{
						if (lineBuffer[0] == '\0')
						{
							// Header has finished
							status = Downloading;
							internalStatus = ReceiveContent;
							break;
						}

						if (!strncmp(lineBuffer, "Location: ", 10))
						{
							if (responseCode == RESPONSE_MOVED_PERMANENTLY || responseCode == RESPONSE_MOVED_TEMPORARILY || responseCode == RESPONSE_TEMPORARY_REDIRECTION || responseCode == RESPONSE_PERMANENT_REDIRECT)
							{
								//printf("Redirecting to %s", lineBuffer + 10);
								//getchar();
								Stop();
								Open(lineBuffer + 10);
								break;
							}
						}

						//printf("Header: %s  -- ", lineBuffer);
						//getchar();
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

bool DOSHTTPRequest::ReadLine()
{
	while (1)
	{
		int rc = sock->recv((unsigned char*) lineBuffer + lineBufferSize, 1);
		if (rc == 0)
		{
			// Need to wait for new packets to be received, defer
			return false;
		}
		else if (rc < 0)
		{
			printf("Receive error\n");
			getchar();
			Error(ContentReceiveError);
			return false;
		}

		if (lineBufferSize >= LINE_BUFFER_SIZE)
		{
			// Line was too long
			lineBuffer[LINE_BUFFER_SIZE - 1] = '\0';
			Error(ContentReceiveError);
			return false;
		}

		if (lineBuffer[lineBufferSize] == '\n')
		{
			lineBuffer[lineBufferSize] = '\0';
			if (lineBufferSize >= 1)
			{
				if (lineBuffer[lineBufferSize - 1] == '\r')
				{
					lineBuffer[lineBufferSize - 1] = '\0';
				}
			}

			lineBufferSize = 0;
			return true;
		}

		if (lineBuffer[lineBufferSize] == '\0')
		{
			// Found terminated string
			lineBufferSize = 0;
			return true;
		}

		lineBufferSize++;
	}
}

const char* DOSHTTPRequest::GetStatusString()
{
	switch (status)
	{
	case HTTPRequest::Error:
		switch (internalStatus)
		{
		case InvalidPort:
			return "Invalid port";
		case InvalidProtocol:
			return "Invalid protocol";
		case SocketCreationError:
			return "Socket creation error";
		case SocketConnectionError:
			return "Socket connection error";
		case HeaderSendError:
			return "Error sending HTTP header";
		case ContentReceiveError:
			return "Error receiving HTTP content";
		case UnsupportedHTTPError:
			return "Unsupported HTTP version";
		case MalformedHTTPVersionLineError:
			return "Malformed HTTP version line";
		case WriteLineError:
			return "Error writing headers";
		}
		break;

	case HTTPRequest::Connecting:
		switch (internalStatus)
		{
		case QueuedDNSRequest:
		case WaitingDNSResolve:
			return "Resolving host name via DNS";
		case OpeningSocket:
			return "Connecting to server";
		case ConnectingSocket:
		case SendHeaders:
			return "Sending headers";
		case ReceiveHeaderResponse:
		case ReceiveHeaderContent:
			return "Receiving headers";
		case ReceiveContent:
			return "Receiving content";
		}
		break;
	default:
		break;
	}
	return "";
}
