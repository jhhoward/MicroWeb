#include <stdio.h>
#include "App.h"
#include "Platform.h"
#include "KeyCodes.h"

App::App() 
	: page(*this), renderer(*this), parser(page)
{
	hoverWidget = NULL;
	oldMouseX = -1;
	oldMouseY = -1;
	requestedNewPage = false;
}

App::~App()
{

}


void App::ResetPage()
{
	page.Reset();
	renderer.Reset();
	parser.Reset();
}

void App::Run()
{
	while (1)
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
				renderer.DrawAddress(page.pageURL.url);
			}

			static char buffer[256];

			size_t bytesRead = loadTask.GetContent(buffer, 256);
			if (bytesRead)
			{
				parser.Parse(buffer, bytesRead);
			}

		}

		{
			int buttons, mouseX, mouseY;
			Platform::input->GetMouseStatus(buttons, mouseX, mouseY);

			Widget* oldHoverWidget = hoverWidget;

			if (hoverWidget && !renderer.IsOverPageWidget(hoverWidget, mouseX, mouseY))
			{
				hoverWidget = renderer.PickPageWidget(mouseX, mouseY);
			}
			else if (!hoverWidget && (mouseX != oldMouseX || mouseY != oldMouseY))
			{
				hoverWidget = renderer.PickPageWidget(mouseX, mouseY);
			}

			oldMouseX = mouseX;
			oldMouseY = mouseY;

			if (hoverWidget != oldHoverWidget)
			{
				if (hoverWidget && hoverWidget->GetLinkURL())
				{
					renderer.DrawStatus(URL::GenerateFromRelative(page.pageURL.url, hoverWidget->GetLinkURL()).url);
					Platform::input->SetMouseCursor(MouseCursor::Hand);
				}
				else
				{
					renderer.DrawStatus("");
					Platform::input->SetMouseCursor(MouseCursor::Pointer);
				}
			}
		}

		InputButtonCode keyPress;
		int scrollDelta = 0;

		while (keyPress = Platform::input->GetKeyPress())
		{
			switch (keyPress)
			{
			case KEYCODE_MOUSE_LEFT:
				if (hoverWidget && hoverWidget->GetLinkURL())
				{
					OpenURL(URL::GenerateFromRelative(page.pageURL.url, hoverWidget->GetLinkURL()).url);
				}
				break;
			case KEYCODE_ESCAPE:
				return;
			case KEYCODE_ARROW_UP:
				scrollDelta -= 8;
				break;
			case KEYCODE_ARROW_DOWN:
				scrollDelta += 8;
				break;
			case KEYCODE_PAGE_UP:
				scrollDelta -= (Platform::video->windowHeight - 24);
				break;
			case KEYCODE_PAGE_DOWN:
				scrollDelta += (Platform::video->windowHeight - 24);
				break;
			case KEYCODE_HOME:
				renderer.Scroll(-page.GetPageHeight());
				break;
			case KEYCODE_END:
				renderer.Scroll(page.GetPageHeight());
				break;
			case 'n':
				OpenURL("http://68k.news");
				break;
			case 's':
				printf("Load type: %d\n", loadTask.type);
				if (loadTask.type == LoadTask::RemoteFile)
				{
					if (loadTask.request)
					{
						printf("Request status: %d\n", loadTask.request->GetStatus());
					}
					else printf("No active request\n");
				}
				break;
			}
		}

		if (scrollDelta)
		{
			renderer.Scroll(scrollDelta);
		}
	}
}

void LoadTask::Load(const char* targetURL)
{
	url = targetURL;

	type = LoadTask::LocalFile;
	fs = fopen(url.url, "r");

	if (!fs)
	{
		type = LoadTask::RemoteFile;
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

void App::OpenURL(const char* url)
{
	//printf("LOAD:\"%s\"\n", url);
	//getchar();
	StopLoad();
	loadTask.Load(url);

	requestedNewPage = true;

	if (loadTask.type == LoadTask::RemoteFile && !loadTask.request)
	{
		ShowErrorPage("No network interface available");
	}
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
	renderer.DrawAddress(page.pageURL.url);
}
