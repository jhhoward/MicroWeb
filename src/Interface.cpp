#include "Platform.h"
#include "Interface.h"
#include "App.h"
#include "KeyCodes.h"

AppInterface::AppInterface(App& inApp) : app(inApp)
{
	GenerateWidgets();

	hoverWidget = NULL;
	oldMouseX = -1;
	oldMouseY = -1;
}

void AppInterface::DrawInterfaceWidgets()
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		app.renderer.RenderWidget(appInterfaceWidgets[n]);
	}
}

void AppInterface::Update()
{
	int buttons, mouseX, mouseY;
	Platform::input->GetMouseStatus(buttons, mouseX, mouseY);

	Widget* oldHoverWidget = hoverWidget;

	if (hoverWidget && !IsOverWidget(hoverWidget, mouseX, mouseY))
	{
		hoverWidget = PickWidget(mouseX, mouseY);
	}
	else if (!hoverWidget && (mouseX != oldMouseX || mouseY != oldMouseY))
	{
		hoverWidget = PickWidget(mouseX, mouseY);
	}

	oldMouseX = mouseX;
	oldMouseY = mouseY;

	if (hoverWidget != oldHoverWidget)
	{
		if (hoverWidget && hoverWidget->GetLinkURL())
		{
			app.renderer.DrawStatus(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
			Platform::input->SetMouseCursor(MouseCursor::Hand);
		}
		else
		{
			app.renderer.DrawStatus("");
			Platform::input->SetMouseCursor(MouseCursor::Pointer);
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
				app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
			}
			else if (hoverWidget == &backButton)
			{
				app.PreviousPage();
			}
			else if (hoverWidget == &forwardButton)
			{
				app.NextPage();
			}
			break;
		case KEYCODE_ESCAPE:
			app.Close();
			break;
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
			app.renderer.Scroll(-app.page.GetPageHeight());
			break;
		case KEYCODE_END:
			app.renderer.Scroll(app.page.GetPageHeight());
			break;
		case 'n':
			app.OpenURL("http://68k.news");
			break;
		case 'b':
			app.PreviousPage();
			break;
		case 'f':
			app.NextPage();
			break;
		}
	}

	if (scrollDelta)
	{
		app.renderer.Scroll(scrollDelta);
	}
}

void AppInterface::GenerateWidgets()
{
	appInterfaceWidgets[0] = &addressBar;
	appInterfaceWidgets[1] = &scrollBar;
	appInterfaceWidgets[2] = &backButton;
	appInterfaceWidgets[3] = &forwardButton;

	addressBar.type = Widget::TextField;
	addressBar.textField = &addressBarData;

	scrollBar.type = Widget::ScrollBar;

	backButtonData.text = "<";
	backButtonData.form = NULL;
	forwardButtonData.text = ">";
	forwardButtonData.form = NULL;

	backButton.type = Widget::Button;
	backButton.button = &backButtonData;
	backButton.style = WidgetStyle(FontStyle::Bold);
	forwardButton.type = Widget::Button;
	forwardButton.button = &forwardButtonData;
	forwardButton.style = WidgetStyle(FontStyle::Bold);

	Platform::video->ArrangeAppInterfaceWidgets(*this);
}

Widget* AppInterface::PickWidget(int x, int y)
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		Widget* widget = appInterfaceWidgets[n];
		if (x >= widget->x && y >= widget->y && x < widget->x + widget->width && y < widget->y + widget->height)
		{
			return widget;
		}
	}

	return app.renderer.PickPageWidget(x, y);
}

bool AppInterface::IsOverWidget(Widget* widget, int x, int y)
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		if (widget == appInterfaceWidgets[n])
		{
			// This is an interface widget
			return (x >= widget->x && y >= widget->y && x < widget->x + widget->width && y < widget->y + widget->height);
		}
	}

	return app.renderer.IsOverPageWidget(widget, x, y);
}
