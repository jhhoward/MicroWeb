#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "HTTP.h"

HTTPRequest::HTTPRequest() : status(HTTPRequest::Stopped), sock(NULL), requestOptions(NULL), requestType(HTTPRequest::Get)
{
	contentType[0] = '\0';
}

void HTTPRequest::Reset()
{
	lineBufferSize = 0;
	lineBufferSendPos = -1;
	contentType[0] = '\0';
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

void HTTPRequest::ResetTimeOutTimer()
{
	timeout = clock() + HTTP_RESPONSE_TIMEOUT;
}

void HTTPRequest::Open(RequestType type, char* inURL, HTTPOptions* options)
{
	url = inURL;
	requestType = type;
	requestOptions = options;
	Reset();

	if (strnicmp(url.url, "http://", 7) == 0) {

		char* hostnameStart = url.url + 7;

		// Scan ahead for another slash; if there is none then we
		// only have a server name and we should fetch the top
		// level directory.

		char* proxy = getenv("HTTP_PROXY");
		//const char* proxy = "127.0.0.1:8888";
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
		
		// If there is a # in the URL, remove from the path
		char* hashPathPtr = strstr(path, "#");
		if (hashPathPtr)
		{
			*hashPathPtr = '\0';
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

		ResetTimeOutTimer();
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
	if (status == HTTPRequest::Downloading && sock && internalStatus == ReceiveContent)
	{
		if (usingChunkedTransfer && count > chunkSizeRemaining)
		{
			count = chunkSizeRemaining;
		}

		int16_t rc = sock->Receive((unsigned char*)buffer, count);
		if (rc < 0)
		{
			MarkError(ContentReceiveError);
		}
		else if(rc > 0)
		{
			ResetTimeOutTimer();

			size_t bytesRead = (size_t)(rc);
			if (contentRemaining > 0)
			{
				contentRemaining -= bytesRead;
				if (contentRemaining <= 0)
				{
					if(requestOptions && requestOptions->keepAlive)
					{
						status = HTTPRequest::Finished;
					}
					else
					{
						Stop();
					}
					
					return bytesRead;
				}
			}

			if (usingChunkedTransfer)
			{
				chunkSizeRemaining -= bytesRead;
				if (!chunkSizeRemaining)
				{
					internalStatus = ParseChunkHeaderLineBreak;
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
	if ((status == HTTPRequest::Connecting || status == HTTPRequest::Downloading) && clock() > timeout)
	{
		//MarkError(TimedOut);
		return;
	}

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
					if (rc > 0)
					{
						internalStatus = WaitingDNSResolve;
					}
					else if (rc == 0)
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
					else if (rc < 0)
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
					ResetTimeOutTimer();
				}
				break;
				case ConnectingSocket:
				{
					if (sock->IsConnectComplete())
					{
						internalStatus = SendHeaders;
						ResetTimeOutTimer();
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
					switch (requestType)
					{
					case HTTPRequest::Post:
						WriteLine("POST %s HTTP/1.1", path);
						break;
					case HTTPRequest::Get:
						WriteLine("GET %s HTTP/1.1", path);
						break;
					}
					WriteLine("User-Agent: MicroWeb " __DATE__);
					WriteLine("Host: %s", hostname);
					WriteLine("Accept-Encoding: identity");
			
					if(requestOptions && requestOptions->keepAlive)
					{
						WriteLine("Connection: keep-alive");
					}
					else
					{
						WriteLine("Connection: close");
					}
			
					if(requestOptions && requestOptions->headerParams)
					{
						WriteLine("%s", requestOptions->headerParams);
					}
			
					if(requestOptions && requestOptions->postContentType && requestOptions->contentData)
					{
						WriteLine("Content-Type: %s", requestOptions->postContentType);
						WriteLine("Content-Length: %d", strlen(requestOptions->contentData));
						WriteLine("");
						WriteLine("%s", requestOptions->contentData);
						WriteLine("");
					}
					else
					{
						WriteLine("");
					}
			
					if(status != HTTPRequest::Error)
					{
						internalStatus = ReceiveHeaderResponse;
					}
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
						usingChunkedTransfer = false;
						contentType[0] = '\0';
					}
				}
				break;
				case ReceiveHeaderContent:
				{
					if (ReadLine())
					{
						if (lineBuffer[0] == '\0')
						{
							if (contentRemaining == 0)
							{
								// Received header with zero content
								MarkError(ContentReceiveError);
							}
							else
							{
								// Header has finished
								if (usingChunkedTransfer)
								{
									internalStatus = ParseChunkHeader;
								}
								else
								{
									status = Downloading;
									internalStatus = ReceiveContent;
								}
							}
							break;
						}
						else if (!strnicmp(lineBuffer, "Location: ", 10))
						{
							if (responseCode == RESPONSE_MOVED_PERMANENTLY || responseCode == RESPONSE_MOVED_TEMPORARILY || responseCode == RESPONSE_TEMPORARY_REDIRECTION || responseCode == RESPONSE_PERMANENT_REDIRECT)
							{
								//printf("Redirecting to %s", lineBuffer + 10);
								//getchar();
								Stop();

								char* redirectedAddress = lineBuffer + 10;
								if (!strnicmp(redirectedAddress, "https://", 8) && !strnicmp(url.url, "http://", 7))
								{
									// Check if the redirected address is http -> https
									if (!strcmp(url.url + 7, redirectedAddress + 8))
									{
										url = redirectedAddress;
										status = HTTPRequest::UnsupportedHTTPS;
										break;
									}
									else
									{
										// Attempt to change this to http:// instead
										strcpy(redirectedAddress + 4, redirectedAddress + 5);
									}
								}

								Open(requestType, redirectedAddress);
								break;
							}
						}
						else if (!strnicmp(lineBuffer, "Content-Length:", 15))
						{
							contentRemaining = strtol(lineBuffer + 15, NULL, 10);
						}
						else if (!stricmp(lineBuffer, "Transfer-Encoding: chunked"))
						{
							usingChunkedTransfer = true;
						}
						else if (!strnicmp(lineBuffer, "Content-Type:", 13))
						{
							strncpy(contentType, lineBuffer + 14, MAX_CONTENT_TYPE_LENGTH);
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
		}
		break;
	}

	if (internalStatus == ParseChunkHeaderLineBreak)
	{
		if (ReadLine())
		{
			internalStatus = ParseChunkHeader;
		}
	}
	if (internalStatus == ParseChunkHeader)
	{
		if (ReadLine())
		{
			chunkSizeRemaining = strtol(lineBuffer, NULL, 16);

			if (chunkSizeRemaining)
			{
				status = Downloading;
				internalStatus = ReceiveContent;
			}
			else
			{
				status = Finished;
			}
		}
	}
}

bool HTTPRequest::ReadLine()
{
	bool allowBufferTruncation = true;

	if (!sock)
		return false;

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
		case TimedOut:
			return "Connection timed out";
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
