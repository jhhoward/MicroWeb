#ifndef HTTP_H_
#define HTTP_H_

#include <time.h>
#include "Platform.h"
#include "URL.h"

#define HOSTNAME_LEN        (80)
#define PATH_LEN           (MAX_URL_LENGTH)
#define LINE_BUFFER_SIZE 512

#define RESPONSE_MOVED_PERMANENTLY 301
#define RESPONSE_MOVED_TEMPORARILY 302
#define RESPONSE_TEMPORARY_REDIRECTION 307
#define RESPONSE_PERMANENT_REDIRECT 308

#define MAX_CONTENT_TYPE_LENGTH 32

#define HTTP_RESPONSE_TIMEOUT_SECONDS 20
#define HTTP_RESPONSE_TIMEOUT (HTTP_RESPONSE_TIMEOUT_SECONDS * CLOCKS_PER_SEC)

struct HTTPOptions
{
	HTTPOptions() : postContentType(0), contentData(0), headerParams(0), keepAlive(false)
	{
	}
	
	const char* postContentType;
	const char* contentData;
	const char* headerParams;
	bool keepAlive;
};

class HTTPRequest
{
public:
	enum Status
	{
		Stopped,
		Connecting,
		Downloading,
		Finished,
		Error,
		UnsupportedHTTPS
	};
	
	enum RequestType
	{
		Get,
		Post
	};

	HTTPRequest();

	void Open(RequestType requestType, char* url, HTTPOptions* options = NULL);

	HTTPRequest::Status GetStatus() { return status; }
	size_t ReadData(char* buffer, size_t count);
	void Stop();
	void Update();
	const char* GetStatusString();
	const char* GetURL() { return url.url; }
	const char* GetContentType() { return contentType; }

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
		UnsupportedHTTPError,
		MalformedHTTPVersionLineError,
		WriteLineError,
		TimedOut,
		HostNameResolveError,

		// Connection states
		QueuedDNSRequest,
		WaitingDNSResolve,
		OpeningSocket,
		ConnectingSocket,
		SendHeaders,
		ReceiveHeaderResponse,
		ReceiveHeaderContent,
		ReceiveContent,
		ParseChunkHeaderLineBreak,
		ParseChunkHeader
	};

	void MarkError(InternalStatus statusError);
	bool ReadLine();
	void WriteLine(const char* fmt, ...);
	bool SendPendingWrites();

	void Reset();
	void ResetTimeOutTimer();

	HTTPRequest::Status status;
	InternalStatus internalStatus;

	URL url;
	char hostname[HOSTNAME_LEN];
	char path[PATH_LEN];
	NetworkAddress hostAddr;
	uint16_t serverPort;
	NetworkTCPSocket* sock;
	int responseCode;
	char contentType[MAX_CONTENT_TYPE_LENGTH];

	HTTPOptions* requestOptions;

	char lineBuffer[LINE_BUFFER_SIZE];
	int lineBufferSize;
	int lineBufferSendPos;

	long contentRemaining;

	long chunkSizeRemaining;
	bool usingChunkedTransfer;
	
	clock_t timeout;
	RequestType requestType;
};


#endif

