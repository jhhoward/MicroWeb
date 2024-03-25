#ifndef HTTP_H_
#define HTTP_H_

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

	HTTPRequest();

	void Open(char* url);

	virtual HTTPRequest::Status GetStatus() { return status; }
	virtual size_t ReadData(char* buffer, size_t count);
	virtual void Stop();
	void Update();
	virtual const char* GetStatusString();
	virtual const char* GetURL() { return url.url; }
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
		ParseChunkHeader
	};

	void MarkError(InternalStatus statusError);
	bool ReadLine();
	void WriteLine(const char* fmt, ...);
	bool SendPendingWrites();

	void Reset();

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

	char lineBuffer[LINE_BUFFER_SIZE];
	int lineBufferSize;
	int lineBufferSendPos;

	long contentRemaining;

	long chunkSizeRemaining;
	bool usingChunkedTransfer;
};


#endif

