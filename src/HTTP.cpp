#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "HTTP.h"

HTTPRequest::HTTPRequest() : status(HTTPRequest::Stopped), sock(NULL)
{
}

void HTTPRequest::Reset()
{
	lineBufferSize = 0;
	lineBufferSendPos = -1;
}

void HTTPRequest::WriteLine(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int maxWriteSize = LINE_BUFFER_SIZE - lineBufferSize - 2;
	int vsrc = vsnprintf(lineBuffer + lineBufferSize, maxWriteSize, fmt, ap);
	va_end(ap);

	if ((vsrc < 0) || (vsrc >= maxWriteSize))
	{
		MarkError(WriteLineError);
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

bool HTTPRequest::SendPendingWrites()
{
	if (sock)
	{
		if (lineBufferSendPos < 0)
		{
			return false;
		}

		int rc = sock->Send((uint8_t*)(lineBuffer + lineBufferSendPos), lineBufferSize - lineBufferSendPos);
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
			MarkError(WriteLineError);
			lineBufferSendPos = -1;
		}
		return lineBufferSendPos >= 0;
	}
	return false;
}

void HTTPRequest::Open(char* inURL)
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
				MarkError(InvalidPort);
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
		MarkError(InvalidProtocol);
	}
}

size_t HTTPRequest::ReadData(char* buffer, size_t count)
{
	if (status == HTTPRequest::Downloading && sock)
	{
		int16_t rc = sock->Receive((unsigned char*)buffer, count);
		if (rc < 0)
		{
			MarkError(ContentReceiveError);
		}
		else
		{
			size_t bytesRead = (size_t)(rc);
			if (contentRemaining > 0)
			{
				contentRemaining -= bytesRead;
				if (contentRemaining <= 0)
				{
					Stop();
				}
			}

			return bytesRead;
		}
	}
	return 0;
}

void HTTPRequest::Stop()
{
	if (sock)
	{
		sock->Close();
		Platform::network->DestroySocket(sock);
		sock = NULL;
	}
	status = HTTPRequest::Stopped;
}

void HTTPRequest::MarkError(InternalStatus statusError)
{
	status = HTTPRequest::Error;
	internalStatus = statusError;
}

void HTTPRequest::Update()
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
			int rc = Platform::network->ResolveAddress(hostname, hostAddr, true);
			if(rc > 0)
			{
				internalStatus = WaitingDNSResolve;
			}
			else if(rc == 0)
			{
				internalStatus = OpeningSocket;
			}
			else
			{
				MarkError(HostNameResolveError);
			}
		}
		break;
		case WaitingDNSResolve:
		{
			int8_t rc = Platform::network->ResolveAddress(hostname, hostAddr, false);
			if (rc == 0)
			{
				internalStatus = OpeningSocket;
			}
			else if(rc < 0)
			{
				MarkError(HostNameResolveError);
			}
		}
		break;
		case OpeningSocket:
		{
			sock = Platform::network->CreateSocket();
			if (!sock)
			{
				MarkError(SocketCreationError);
			}

			if (sock->Connect(hostAddr, serverPort))
			{
				MarkError(SocketCreationError);
				break;
			}
			internalStatus = ConnectingSocket;
		}
		break;
		case ConnectingSocket:
		{
			if (sock->IsConnectComplete())
			{
				internalStatus = SendHeaders;
				break;
			}
			else if (sock->IsClosed())
			{
				MarkError(SocketConnectionError);
				break;
			}
		}
		break;
		case SendHeaders:
		{
			WriteLine("GET %s HTTP/1.1", path);
			WriteLine("User-Agent: MicroWeb " __DATE__);
			WriteLine("Host: %s", hostname);
			WriteLine("Connection: close");
			WriteLine("");
			internalStatus = ReceiveHeaderResponse;
		}
		break;
		case ReceiveHeaderResponse:
		{
			if (ReadLine())
			{
				if ((strncmp(lineBuffer, "HTTP/1.0", 8) != 0) && (strncmp(lineBuffer, "HTTP/1.1", 8) != 0)) {
					MarkError(UnsupportedHTTPError);
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
					MarkError(MalformedHTTPVersionLineError);
					return;
				}

				//printf("Response code: %d", responseCode);
				//getchar();
				internalStatus = ReceiveHeaderContent;

				contentRemaining = -1;
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
				if (!strncmp(lineBuffer, "Content-Length:", 15))
				{
					contentRemaining = atoi(lineBuffer + 15);
				}

				//printf("Header: %s  -- \n", lineBuffer);
				//getchar();
			}
		}
		break;
		}
	}
	break;

	case HTTPRequest::Downloading:
	{
		//if (sock->isRemoteClosed())
		//{
		//	//status = HTTPRequest::Finished;
		//}
	}
	break;
	}
}

bool HTTPRequest::ReadLine()
{
	bool allowBufferTruncation = true;

	while (1)
	{
		int rc = sock->Receive((unsigned char*)lineBuffer + lineBufferSize, 1);
		if (rc == 0)
		{
			// Need to wait for new packets to be received, defer
			return false;
		}
		else if (rc < 0)
		{
			printf("Receive error\n");
			MarkError(ContentReceiveError);
			return false;
		}

		if (lineBufferSize >= LINE_BUFFER_SIZE)
		{
			// Line was too long
			lineBuffer[LINE_BUFFER_SIZE - 1] = '\0';
			MarkError(ContentReceiveError);
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

		if (!allowBufferTruncation || lineBufferSize < LINE_BUFFER_SIZE - 1)
		{
			lineBufferSize++;
		}
	}
}

const char* HTTPRequest::GetStatusString()
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
		case HostNameResolveError:
			return "Error resolving host name";
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
