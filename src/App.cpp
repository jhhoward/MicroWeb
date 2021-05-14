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

void App::Run()
{
	while (1)
	{
		Platform::network->Update();

		if (loadTask.type == LoadTask::LocalFile)
		{
			if (loadTask.fs && !feof(loadTask.fs))
			{
				char buffer[50];

				size_t bytesRead = fread(buffer, 1, 50, loadTask.fs);
				if (bytesRead)
				{
					parser.Parse(buffer, bytesRead);
				}
			}
		}
		else if (loadTask.type == LoadTask::RemoteFile)
		{
			if (loadTask.request && loadTask.request->GetStatus() == HTTPRequest::Downloading)
			{
				char buffer[50];

				size_t bytesRead = loadTask.request->ReadData(buffer, 50);
				if (bytesRead)
				{
					parser.Parse(buffer, bytesRead);
				}
			}
		}

		{
			int buttons, mouseX, mouseY;
			Platform::mouse->GetMouseState(buttons, mouseX, mouseY);
			if (buttons == 2)
			{
				break;
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
					renderer.DrawStatus(hoverWidget->linkURL);
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

void App::OpenURL(char* url)
{
	loadTask.fs = fopen(url, "r");
	loadTask.type = LoadTask::LocalFile;

	if (!loadTask.fs)
	{
		loadTask.type = LoadTask::RemoteFile;
		loadTask.request = Platform::network->CreateRequest(url);
	}
}

