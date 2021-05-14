#pragma once

#include <stdio.h>
#include "Parser.h"
#include "Page.h"
#include "Renderer.h"
#include "URL.h"

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

class Widget;

class App
{
public:
	App();
	~App();

	void Run();
	void OpenURL(const char* url);

	void StopLoad();

	Page page;
	Renderer renderer;
	HTMLParser parser;

private:
	void ResetPage();

	bool requestedNewPage;
	LoadTask loadTask;
	Widget* hoverWidget;
	int oldMouseX, oldMouseY;

//	URL pageHistory[MAX_PAGE_URL_HISTORY];
//	int pageHistorySize;
//	int pageHistoryPos;
};
