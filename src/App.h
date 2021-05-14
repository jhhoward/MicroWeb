#pragma once

#include <stdio.h>
#include "Parser.h"
#include "Page.h"
#include "Renderer.h"

class HTTPRequest;

struct LoadTask
{
	LoadTask() : type(LocalFile), fs(NULL) {}

	enum Type
	{
		LocalFile,
		RemoteFile,
	};

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
	void OpenURL(char* url);

	Page page;
	Renderer renderer;
	HTMLParser parser;

private:
	LoadTask loadTask;
	Widget* hoverWidget;
	int oldMouseX, oldMouseY;
};
