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
		ReceiveContent
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

	char lineBuffer[LINE_BUFFER_SIZE];
	int lineBufferSize;
	int lineBufferSendPos;

	long contentRemaining;
};


#endif

