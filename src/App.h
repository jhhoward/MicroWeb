#pragma once

#include <stdio.h>
#include "Parser.h"
#include "Page.h"
#include "Renderer.h"

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

	Page page;
	Renderer renderer;
	HTMLParser parser;

private:
	LoadTask loadTask;
	Widget* hoverWidget;
	int oldMouseX, oldMouseY;
};
