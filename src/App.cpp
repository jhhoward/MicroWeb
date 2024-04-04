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

#include <stdio.h>
#include "App.h"
#include "Platform.h"
#include "HTTP.h"
#include "Image/Decoder.h"

App* App::app;
AppConfig App::config;

App::App() 
	: page(*this), pageRenderer(*this), parser(page), ui(*this)
{
	app = this;
	requestedNewPage = false;

	memset(pageHistoryBuffer, 0, MAX_PAGE_HISTORY_BUFFER_SIZE);
	pageHistoryPtr = pageHistoryBuffer;
}

App::~App()
{
	app = NULL;
}


void App::ResetPage()
{
	StylePool::Get().Reset();
	page.Reset();
	parser.Reset();
	pageRenderer.Reset();
	ui.Reset();
	pageRenderer.RefreshAll();
}

void App::Run(int argc, char* argv[])
{
	running = true;
	char* targetURL = nullptr;

	config.loadImages = true;
	config.dumpPage = false;
	config.useSwap = false;
	config.useEMS = true;

	if (argc > 1)
	{
		for (int n = 1; n < argc; n++)
		{
			if (*argv[n] != '-')
			{
				targetURL = argv[n];
				break;
			}
			else if (!stricmp(argv[n], "-noimages"))
			{
				config.loadImages = false;
			}
			else if (!stricmp(argv[n], "-dumppage"))
			{
				config.dumpPage = true;
			}
			else if (!stricmp(argv[n], "-i"))
			{
				config.invertScreen = true;
			}
			else if (!stricmp(argv[n], "-useswap"))
			{
				config.useSwap = true;
			}
			else if (!stricmp(argv[n], "-noems"))
			{
				config.useEMS = false;
			}
		}
	}

	MemoryManager::pageBlockAllocator.Init();

	if (config.loadImages)
	{
		ImageDecoder::Allocate();
	}

	StylePool::Get().Init();
	ui.Init();
	page.Reset();
	pageRenderer.Init();

	if (targetURL)
	{
		OpenURL(targetURL);
	}
	else
	{
		ui.FocusNode(ui.addressBarNode);
	}

	while (running)
	{
		Platform::Update();

		if (pageLoadTask.HasContent())
		{
			if (requestedNewPage)
			{
				ResetPage();
				requestedNewPage = false;
				page.pageURL = pageLoadTask.GetURL();
				ui.UpdateAddressBar(page.pageURL);
				loadTaskTargetNode = page.GetRootNode();
				ui.SetStatusMessage("Parsing page content...", StatusBarNode::GeneralStatus);
			}

			size_t bytesRead = pageLoadTask.GetContent(loadBuffer, APP_LOAD_BUFFER_SIZE);
			if (bytesRead)
			{
				if (pageLoadTask.debugDumpFile)
				{
					fwrite(loadBuffer, 1, bytesRead, pageLoadTask.debugDumpFile);
				}

				parser.Parse(loadBuffer, bytesRead);
			}
		}
		else
		{
			if (requestedNewPage)
			{
				page.pageURL = pageLoadTask.GetURL();
				ui.UpdateAddressBar(page.pageURL);

				if (pageLoadTask.type == LoadTask::RemoteFile)
				{
					if (!pageLoadTask.request)
					{
						if (Platform::network->IsConnected())
						{
							ShowErrorPage("Failed to make network request");
						}
						else
						{
							ShowErrorPage("No network interface available");
						}
						requestedNewPage = false;
					}
					else if (pageLoadTask.request->GetStatus() == HTTPRequest::Error)
					{
						ShowErrorPage(pageLoadTask.request->GetStatusString());
						requestedNewPage = false;
					}
					else if (pageLoadTask.request->GetStatus() == HTTPRequest::UnsupportedHTTPS)
					{
						ShowNoHTTPSPage();
						requestedNewPage = false;
					}
				}
				else if (pageLoadTask.type == LoadTask::LocalFile)
				{
					ShowErrorPage("File not found");
					requestedNewPage = false;
				}
			}
			else if (!parser.IsFinished())
			{
				parser.Finish();
			}
		}

		if (pageContentLoadTask.HasContent())
		{
			size_t bytesRead = pageContentLoadTask.GetContent(loadBuffer, APP_LOAD_BUFFER_SIZE);
			if (bytesRead)
			{
				bool stillProcessing = loadTaskTargetNode->Handler().ParseContent(loadTaskTargetNode, loadBuffer, bytesRead);
				if (!stillProcessing)
				{
					pageContentLoadTask.Stop();
				}
			}
		}
		else if(!pageContentLoadTask.IsBusy())
		{
			if (loadTaskTargetNode)
			{
				loadTaskTargetNode->Handler().FinishContent(loadTaskTargetNode, pageContentLoadTask);
				loadTaskTargetNode = page.ProcessNextLoadTask(loadTaskTargetNode, pageContentLoadTask);

				if (!loadTaskTargetNode && page.layout.IsFinished())
				{
					if (MemoryManager::pageAllocator.GetError())
					{
						page.GetApp().ui.SetStatusMessage("Out of memory when loading page", StatusBarNode::GeneralStatus);
					}
					else
					{
						page.GetApp().ui.ClearStatusMessage(StatusBarNode::GeneralStatus);
					}
				}
			}
		}
		//if (loadTask.type == LoadTask::RemoteFile && loadTask.request && loadTask.request->GetStatus() == HTTPRequest::Connecting)
		//	ui.SetStatusMessage(loadTask.request->GetStatusString());

		page.layout.Update();
		pageRenderer.Update();
		ui.Update();
	}
}

void LoadTask::Load(const char* targetURL)
{
	Stop();

	url = targetURL;

	// Check for protocol substring
	if (strstr(url.url, "http://") == url.url)
	{
		type = LoadTask::RemoteFile;
	}
	else if (strstr(url.url, "file://") == url.url)
	{
		type = LoadTask::LocalFile;
		fs = fopen(url.url + 7, "rb");
	}
	else if (strstr(url.url, "https://") == url.url)
	{
		type = LoadTask::RemoteFile;

		// Bit of a hack: try forcing http:// first
		strcpy(url.url + 4, url.url + 5);
	}
	else if (strstr(url.url, "://"))
	{
		// Will be an unsupported protocol
		type = LoadTask::RemoteFile;
	}
	else
	{
		// User did not include protocol, first check for local file
		type = LoadTask::LocalFile;
		fs = fopen(targetURL, "rb");

		if (fs)
		{
			// Local file exists, prepend with file:// protocol
			strcpy(url.url, "file://");
			strcpy(url.url + 7, targetURL);
		}
		else
		{
			// did this start with X:\ ?
			if (targetURL[0] >= 'A' && targetURL[0] <= 'z' && targetURL[1] == ':' && targetURL[2] == '\\')
			{
				type = LoadTask::LocalFile;
				fs = nullptr;
			}
			else
			{
				// Assume this should be http://
				type = LoadTask::RemoteFile;
				strcpy(url.url, "http://");
				strcpy(url.url + 7, targetURL);
			}
		}
	}

	url.CleanUp();

	if (type == LoadTask::RemoteFile)
	{
		request = Platform::network->CreateRequest(url.url);

		if (App::config.dumpPage && this == &App::Get().pageLoadTask)
		{
			debugDumpFile = fopen("dump.htm", "wb");
		}
	}
}

void LoadTask::Stop()
{
	if (debugDumpFile)
	{
		fclose(debugDumpFile);
		debugDumpFile = NULL;
	}

	switch (type)
	{
	case LoadTask::LocalFile:
		if (fs)
		{
			fclose(fs);
			fs = nullptr;
		}
		break;
	case LoadTask::RemoteFile:
		if (request)
		{
			request->Stop();
			Platform::network->DestroyRequest(request);
			request = nullptr;
		}
		break;
	}
}

const char* LoadTask::GetURL()
{
	if (type == LoadTask::RemoteFile && request)
	{
		return request->GetURL();
	}
	return url.url;
}

bool LoadTask::IsBusy()
{
	bool isStillConnecting = type == LoadTask::RemoteFile && request && request->GetStatus() == HTTPRequest::Connecting;
	return isStillConnecting || HasContent();
}

bool LoadTask::HasContent()
{
	if (type == LoadTask::LocalFile)
	{
		return (fs && !feof(fs));
	}
	else if (type == LoadTask::RemoteFile)
	{
		return (request && request->GetStatus() == HTTPRequest::Downloading);
	}
	return false;
}

size_t LoadTask::GetContent(char* buffer, size_t count)
{
	if (type == LoadTask::LocalFile)
	{
		if (fs)
		{
			if (!feof(fs))
			{
				return fread(buffer, 1, count, fs);
			}
			fclose(fs);
		}
	}
	else if (type == LoadTask::RemoteFile)
	{
		if (request)
		{ 
			switch (request->GetStatus())
			{
			case HTTPRequest::Downloading:
				return request->ReadData(buffer, count);
			case HTTPRequest::Error:
			case HTTPRequest::Finished:
			case HTTPRequest::Stopped:
				Stop();
				break;
			}
		}
	}
	return 0;
}

void App::RequestNewPage(const char* url)
{
	if (!url || !*url)
		return;

	StopLoad();
	pageLoadTask.Load(url);
	requestedNewPage = true;
	loadTaskTargetNode = nullptr;
	//ui.UpdateAddressBar(loadTask.url);

	if (pageLoadTask.type == LoadTask::RemoteFile && pageLoadTask.request)
	{
		ui.SetStatusMessage("Connecting to server...", StatusBarNode::GeneralStatus);
	}
}

void App::OpenURL(const char* url)
{
	RequestNewPage(url);

	size_t urlStringLength = strlen(url) + 1;

	if (*pageHistoryPtr)
	{
		pageHistoryPtr += strlen(pageHistoryPtr) + 1;
	}

	// If not enough space in the buffer then move to previous space
	while (pageHistoryPtr + urlStringLength > pageHistoryBuffer + MAX_PAGE_HISTORY_BUFFER_SIZE)
	{
		do
		{
			pageHistoryPtr--;
		} while (pageHistoryPtr > pageHistoryBuffer && pageHistoryPtr[-1]);
	}

	strcpy(pageHistoryPtr, url);
	memset(pageHistoryPtr + urlStringLength, 0, MAX_PAGE_HISTORY_BUFFER_SIZE - (pageHistoryPtr + urlStringLength - pageHistoryBuffer));
}

void App::StopLoad()
{
	pageLoadTask.Stop();
	pageContentLoadTask.Stop();
}

void App::ShowErrorPage(const char* message)
{
	StopLoad();
	ResetPage();

	page.SetTitle("Error");
//	page.pageURL = "about:error";
//	ui.UpdateAddressBar(page.pageURL);

	parser.Write("<html>");
	parser.Write("<h1>Error loading page</h1>");
	parser.Write("<hr>");
	parser.Write(message);
	parser.Write("</html>");
	parser.Finish();
}

static const char* frogFindURL = "http://frogfind.com/read.php?a=";
#define FROG_FIND_URL_LENGTH 31

void App::ShowNoHTTPSPage()
{
	ResetPage();

	page.SetTitle("HTTPS unsupported");
	page.pageURL = pageLoadTask.GetURL();
	ui.UpdateAddressBar(page.pageURL);

	StopLoad();

	page.pageURL = frogFindURL;
	strncpy(page.pageURL.url + FROG_FIND_URL_LENGTH, pageLoadTask.GetURL(), MAX_URL_LENGTH - FROG_FIND_URL_LENGTH);

	parser.Write("<html>");
	parser.Write("<h1>HTTPS unsupported</h1>");
	parser.Write("<hr>");
	parser.Write("Sorry this browser does not support HTTPS!<br>");
	parser.Write("<a href=\"");
	parser.Write(page.pageURL.url);
	parser.Write("\">Visit this site via FrogFind</a>");
	parser.Write("</html>");
	parser.Finish();
}

void App::PreviousPage()
{
	if (pageHistoryPtr > pageHistoryBuffer)
	{
		do
		{
			pageHistoryPtr--;
		} while (pageHistoryPtr > pageHistoryBuffer && pageHistoryPtr[-1]);

		RequestNewPage(pageHistoryPtr);
	}
}

void App::NextPage()
{
	if (*pageHistoryPtr)
	{
		char* next = pageHistoryPtr + strlen(pageHistoryPtr) + 1;
		if (next < pageHistoryBuffer + MAX_PAGE_HISTORY_BUFFER_SIZE && *next)
		{
			pageHistoryPtr = next;
			RequestNewPage(pageHistoryPtr);
		}
	}
}

void App::ReloadPage()
{
	if (*pageHistoryPtr)
	{
		RequestNewPage(pageHistoryPtr);
	}
}

void App::LoadImageNodeContent(Node* node)
{
	loadTaskTargetNode = node;
	node->Handler().LoadContent(node, pageContentLoadTask);
}

void VideoDriver::InvertVideoOutput()
{
	if (drawSurface->bpp == 1)
	{
		App::config.invertScreen = !App::config.invertScreen;

		DrawContext context(drawSurface, 0, 0, screenWidth, screenHeight);
		Platform::input->HideMouse();
		context.surface->InvertRect(context, 0, 0, screenWidth, screenHeight);
		Platform::input->ShowMouse();
	}
}
