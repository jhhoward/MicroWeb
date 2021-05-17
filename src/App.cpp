#include <stdio.h>
#include "App.h"
#include "Platform.h"

#include <conio.h>

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

#define KEYCODE_ARROW_UP 72
#define KEYCODE_ARROW_DOWN 80

#define KEYCODE_PAGE_UP 73
#define KEYCODE_PAGE_DOWN 81

#define KEYCODE_HOME 71
#define KEYCODE_END 79

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
		Platform::network->Update();

		if (loadTask.HasContent())
		{
			if (requestedNewPage)
			{
				ResetPage();
				requestedNewPage = false;
				page.pageURL = loadTask.url;
				renderer.DrawAddress(page.pageURL.url);
			}

			char buffer[50];

			size_t bytesRead = loadTask.GetContent(buffer, 50);
			if (bytesRead)
			{
				parser.Parse(buffer, bytesRead);
			}

		}

		{
			int buttons, mouseX, mouseY;
			Platform::mouse->GetMouseState(buttons, mouseX, mouseY);
			if (buttons == 2)
			{
				break;
			}

			if (buttons == 1)
			{
				while (buttons)
				{
					Platform::mouse->GetMouseState(buttons, mouseX, mouseY);
				}
				if (hoverWidget && hoverWidget->linkURL)
				{
					OpenURL(URL::GenerateFromRelative(page.pageURL.url, hoverWidget->linkURL).url);
				}
			}

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
				if (hoverWidget && hoverWidget->linkURL)
				{
					renderer.DrawStatus(URL::GenerateFromRelative(page.pageURL.url, hoverWidget->linkURL).url);
					Platform::mouse->SetCursor(MouseCursor::Hand);
				}
				else
				{
					renderer.DrawStatus("");
					Platform::mouse->SetCursor(MouseCursor::Pointer);
				}
			}
		}

		if (kbhit())
		{
			int keyPress = getch();
			if (keyPress == 27)
			{
				break;
			}
			else
			{
				if (keyPress == 0)
				{
					keyPress = getch();
					//printf("special %d\n", keyPress);

					if (keyPress == KEYCODE_ARROW_UP)
					{
						renderer.Scroll(-8);
					}
					if (keyPress == KEYCODE_ARROW_DOWN)
					{
						renderer.Scroll(8);
					}
					if (keyPress == KEYCODE_PAGE_UP)
					{
						renderer.Scroll(-(Platform::video->windowHeight - 24));
					}
					if (keyPress == KEYCODE_PAGE_DOWN)
					{
						renderer.Scroll((Platform::video->windowHeight - 24));
					}
					if (keyPress == KEYCODE_HOME)
					{
						renderer.Scroll(-page.GetPageHeight());
					}
					if (keyPress == KEYCODE_END)
					{
						renderer.Scroll(page.GetPageHeight());
					}
				}
				else
				{
					if (keyPress >= 32 && keyPress < 128)
					{
						if (keyPress == 'f')
						{
							//OpenURL("http://www.frogfind.com/");
							//OpenURL("examples/sq.htm");
						}
						else if (keyPress == 'n')
						{
							OpenURL("http://68k.news/");
							//OpenURL("examples/test.htm");
						}

						if (keyPress == 's')
						{
							printf("Load type: %d\n", loadTask.type);
							if (loadTask.type == LoadTask::RemoteFile)
							{
								if (loadTask.request)
								{
									printf("Request status: %d\n", loadTask.request->GetStatus());
								}
								else printf("No active request\n");
							}
						}
						//printf("%d - '%c'\n", keyPress, (char)keyPress);
					}
					else
					{
						//printf("%d\n", keyPress);
					}
				}
			}
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
