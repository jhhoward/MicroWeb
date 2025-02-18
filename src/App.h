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

#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include "Parser.h"
#include "Page.h"
#include "URL.h"
#include "Interface.h"
#include "Render.h"
#include "HTTP.h"

#define MAX_PAGE_HISTORY_BUFFER_SIZE MAX_URL_LENGTH
#define APP_LOAD_BUFFER_SIZE 256

class HTTPRequest;
struct HTTPOptions;

struct LoadTask
{
	LoadTask() : type(LocalFile), fs(NULL), debugDumpFile(NULL) {}

	void Load(HTTPRequest::RequestType requestType, const char* url, HTTPOptions* options = NULL);
	void Stop();
	bool HasContent();
	bool IsBusy();
	size_t GetContent(char* buffer, size_t count);
	const char* GetURL();

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

	FILE* debugDumpFile;
};

struct Widget;

struct AppConfig
{
	bool loadImages : 1;
	bool dumpPage : 1;
	bool invertScreen : 1;
	bool useSwap : 1;
	bool useEMS : 1;
};

class App
{
public:
	App();
	~App();

	void Run(int argc, char* argv[]);
	void Close() { running = false; }
	void OpenURL(HTTPRequest::RequestType requestType, const char* url, HTTPOptions* options = NULL);

	void PreviousPage();
	void NextPage();

	void StopLoad();
	void ReloadPage();

	void ShowErrorPage(const char* message);

	static App& Get() { return *app; }

	Page page;
	PageRenderer pageRenderer;
	HTMLParser parser;
	AppInterface ui;
	static AppConfig config;

	LoadTask pageLoadTask;
	LoadTask pageContentLoadTask;

	void LoadImageNodeContent(Node* node);

private:
	void ResetPage();
	void RequestNewPage(HTTPRequest::RequestType requestType, const char* url, HTTPOptions* options = NULL);

	void ShowNoHTTPSPage();

	bool requestedNewPage;
	Node* loadTaskTargetNode;
	bool running;

	char pageHistoryBuffer[MAX_PAGE_HISTORY_BUFFER_SIZE];
	char* pageHistoryPtr;

	static App* app;

	char loadBuffer[APP_LOAD_BUFFER_SIZE];
};


#endif
