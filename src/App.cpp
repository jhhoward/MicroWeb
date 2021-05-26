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

App::App() 
	: page(*this), renderer(*this), parser(page), ui(*this)
{
	requestedNewPage = false;
	pageHistorySize = 0;
	pageHistoryPos = -1;
}

App::~App()
{

}


void App::ResetPage()
{
	page.Reset();
	renderer.Reset();
	parser.Reset();
	ui.Reset();
}

void App::Run()
{
	running = true;

	renderer.Init();

	while (running)
	{
		Platform::Update();
		renderer.Update();

		if (loadTask.HasContent())
		{
			if (requestedNewPage)
			{
				ResetPage();
				requestedNewPage = false;
				page.pageURL = loadTask.url;
				ui.UpdateAddressBar(page.pageURL);
				//renderer.DrawAddress(page.pageURL.url);
			}

			static char buffer[256];

			size_t bytesRead = loadTask.GetContent(buffer, 256);
			if (bytesRead)
			{
				parser.Parse(buffer, bytesRead);
			}
		}
		else
		{
			if (requestedNewPage)
			{
				if (loadTask.type == LoadTask::RemoteFile)
				{
					if (!loadTask.request)
					{
						ShowErrorPage("No network interface available");
						requestedNewPage = false;
					}
					else if (loadTask.request->GetStatus() == HTTPRequest::Error)
					{
						ShowErrorPage(loadTask.request->GetStatusString());
						requestedNewPage = false;
					}
				}				
			}
		}
		if (loadTask.type == LoadTask::RemoteFile && loadTask.request && loadTask.request->GetStatus() == HTTPRequest::Connecting)
			renderer.SetStatus(loadTask.request->GetStatusString());

		ui.Update();
	}
}

void LoadTask::Load(const char* targetURL)
{
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
	/*else if (strstr(url.url, "https://") == url.url)
	{
		strcpy(url.url, "http://");
		strcpy(url.url + 7, targetURL + 8);
		type = LoadTask::RemoteFile;
	}*/
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
			// Assume this should be http://
			type = LoadTask::RemoteFile;
			strcpy(url.url, "http://");
			strcpy(url.url + 7, targetURL);
		}
	}

	if (type == LoadTask::RemoteFile)
	{
		request = Platform::network->CreateRequest(url.url);
	}
}

void LoadTask::Stop()
{
	switch (type)
	{
	case LoadTask::LocalFile:
		if (fs)
		{
			fclose(fs);
			fs = NULL;
		}
		break;
	case LoadTask::RemoteFile:
		if (request)
		{
			request->Stop();
			Platform::network->DestroyRequest(request);
			request = NULL;
		}
		break;
	}
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
		if (fs && !feof(fs))
		{
			return fread(buffer, 1, count, fs);
		}
	}
	else if (type == LoadTask::RemoteFile)
	{
		if (request && request->GetStatus() == HTTPRequest::Downloading)
		{
			return request->ReadData(buffer, count);
		}
	}
	return 0;
}

void App::RequestNewPage(const char* url)
{
	StopLoad();
	loadTask.Load(url);
	requestedNewPage = true;
	//ui.UpdateAddressBar(loadTask.url);
}

void App::OpenURL(const char* url)
{
	RequestNewPage(url);

	pageHistoryPos++;
	if (pageHistoryPos >= MAX_PAGE_URL_HISTORY)
	{
		for (int n = 0; n < MAX_PAGE_URL_HISTORY - 1; n++)
		{
			pageHistory[n] = pageHistory[n + 1];
		}
		pageHistoryPos--;
	}

	pageHistory[pageHistoryPos] = loadTask.url;
	pageHistorySize = pageHistoryPos + 1;
}

void App::StopLoad()
{
	loadTask.Stop();
}

void App::ShowErrorPage(const char* message)
{
	loadTask.Stop();
	ResetPage();

	page.BreakLine(2);
	page.PushStyle(WidgetStyle(FontStyle::Bold, 2));
	page.AppendText("Error");
	page.PopStyle();
	page.BreakLine(2);
	page.AppendText(message);
	page.FinishSection();
	
	page.SetTitle("Error");
	page.pageURL = "about:error";
	ui.UpdateAddressBar(page.pageURL);
	//renderer.DrawAddress(page.pageURL.url);
}

void App::PreviousPage()
{
	if (pageHistoryPos > 0)
	{
		pageHistoryPos--;
		RequestNewPage(pageHistory[pageHistoryPos].url);
	}
}

void App::NextPage()
{
	if (pageHistoryPos + 1 < pageHistorySize)
	{
		pageHistoryPos++;
		RequestNewPage(pageHistory[pageHistoryPos].url);
	}
}
