#pragma once

#include <stdio.h>
#include "Parser.h"
#include "Page.h"
#include "Renderer.h"
#include "URL.h"
#include "Interface.h"

#define MAX_PAGE_URL_HISTORY 5

class HTTPRequest;

struct LoadTask
{
	LoadTask() : type(LocalFile), fs(NULL) {}

	void Load(const char* url);
	void Stop();
	bool HasContent();
	size_t GetContent(char* buffer, size_t count);

	enum Type
	{
		LocalFile,
		RemoteFile,
	};

	URL url;
	Type type;

	union
	{
		FILE* fs;
		HTTPRequest* request;
	};
};

struct Widget;

class App
{
public:
	App();
	~App();

	void Run();
	void Close() { running = false; }
	void OpenURL(const char* url);

	void PreviousPage();
	void NextPage();

	void StopLoad();

	void ShowErrorPage(const char* message);

	Page page;
	Renderer renderer;
	HTMLParser parser;
	AppInterface ui;

private:
	void ResetPage();
	void RequestNewPage(const char* url);

	bool requestedNewPage;
	LoadTask loadTask;
	bool running;

	URL pageHistory[MAX_PAGE_URL_HISTORY];
	int pageHistorySize;
	int pageHistoryPos;
};
