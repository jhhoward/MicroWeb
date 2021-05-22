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

#include "Platform.h"
#include "Interface.h"
#include "App.h"
#include "KeyCodes.h"

#define MIN_SCROLL_WIDGET_SIZE 8

AppInterface::AppInterface(App& inApp) : app(inApp)
{
	GenerateWidgets();

	hoverWidget = NULL;
	oldMouseX = -1;
	oldMouseY = -1;
	oldButtons = 0;
	activeWidget = &addressBar;
}

void AppInterface::Reset()
{
	oldPageHeight = 0;
}

void AppInterface::DrawInterfaceWidgets()
{
	for (int n = 0; n < NUM_APP_INTERFACE_WIDGETS; n++)
	{
		app.renderer.RenderWidget(appInterfaceWidgets[n]);
	}

	// Page top line divider
	Platform::video->HLine(0, Platform::video->windowY - 1, Platform::video->screenWidth);
}

void AppInterface::Update()
{
	if (app.page.GetPageHeight() != oldPageHeight)
	{
		oldPageHeight = app.page.GetPageHeight();
		UpdatePageScrollBar();
	}

	int buttons, mouseX, mouseY;
	Platform::input->GetMouseStatus(buttons, mouseX, mouseY);

	Widget* oldHoverWidget = hoverWidget;

	if (hoverWidget && !app.renderer.IsOverWidget(hoverWidget, mouseX, mouseY))
	{
		hoverWidget = PickWidget(mouseX, mouseY);
	}
	else if (!hoverWidget && (mouseX != oldMouseX || mouseY != oldMouseY))
	{
		hoverWidget = PickWidget(mouseX, mouseY);
	}

	if ((buttons & 1) && !(oldButtons & 1))
	{
		HandleClick(mouseX, mouseY);
	}
	else if (!(buttons & 1) && (oldButtons & 1))
	{
		HandleRelease();
	}

	if (activeWidget == &scrollBar)
	{
		if (mouseY != oldMouseY)
		{
			scrollBar.scrollBar->position = mouseY - scrollBar.y - scrollBarRelativeClickPositionY;
			if (scrollBar.scrollBar->position < 0)
			{
				scrollBar.scrollBar->position = 0;
			}
			if (scrollBar.scrollBar->position + scrollBar.scrollBar->size > scrollBar.height)
			{
				scrollBar.scrollBar->position = scrollBar.height - scrollBar.scrollBar->size;
			}

			Platform::input->HideMouse();
			app.renderer.RedrawScrollBar();
			Platform::input->ShowMouse();
			//Platform::input->SetMousePosition(scrollBar.x + scrollBarRelativeClickPositionX, scrollBar.y + scrollBar.scrollBar->position + scrollBarRelativeClickPositionY);
		}
	}

	oldMouseX = mouseX;
	oldMouseY = mouseY;
	oldButtons = buttons;

	if (hoverWidget != oldHoverWidget)
	{
		if (hoverWidget && hoverWidget->GetLinkURL())
		{
			app.renderer.SetStatus(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
			Platform::input->SetMouseCursor(MouseCursor::Hand);
		}
		else if (hoverWidget && hoverWidget->type == Widget::TextField)
		{
			Platform::input->SetMouseCursor(MouseCursor::TextSelect);
		}
		else
		{
			app.renderer.SetStatus("");
			Platform::input->SetMouseCursor(MouseCursor::Pointer);
		}
	}
	

	InputButtonCode keyPress;
	int scrollDelta = 0;

	while (keyPress = Platform::input->GetKeyPress())
	{
		if (activeWidget)
		{
			if (HandleActiveWidget(keyPress))
			{
				continue;
			}
		}

		switch (keyPress)
		{
//		case KEYCODE_MOUSE_LEFT:
//			HandleClick();
//			break;
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
			app.renderer.ScrollTo(0);
			break;
		case KEYCODE_END:
			app.renderer.ScrollTo(app.renderer.GetMaxScrollPosition());
			break;
		case 'n':
			app.OpenURL("http://68k.news");
			break;
		//case 'b':
		case KEYCODE_BACKSPACE:
			app.PreviousPage();
			break;
		//case 'f':
		//	app.NextPage();
		//	break;
		case KEYCODE_CTRL_I:
		{
			Platform::input->HideMouse();
			Platform::video->InvertScreen();
			Platform::input->ShowMouse();
		}
			break;
		case KEYCODE_CTRL_L:
			activeWidget = &addressBar;
			break;
		default:
			//printf("%d\n", keyPress);
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
	addressBarData.buffer = addressBarURL.url;
	addressBarData.bufferLength = MAX_URL_LENGTH - 1;
	addressBar.style = WidgetStyle(FontStyle::Regular);
	addressBar.isInterfaceWidget = true;

	scrollBar.type = Widget::ScrollBar;
	scrollBar.isInterfaceWidget = true;
	scrollBar.scrollBar = &scrollBarData;
	scrollBarData.position = 0;
	scrollBarData.size = Platform::video->windowHeight;

	backButton.isInterfaceWidget = true;
	backButtonData.text = (char*) "<";
	backButtonData.form = NULL;
	forwardButton.isInterfaceWidget = true;
	forwardButtonData.text = (char*) ">";
	forwardButtonData.form = NULL;

	backButton.type = Widget::Button;
	backButton.button = &backButtonData;
	backButton.style = WidgetStyle(FontStyle::Bold);
	forwardButton.type = Widget::Button;
	forwardButton.button = &forwardButtonData;
	forwardButton.style = WidgetStyle(FontStyle::Bold);

	titleBar.isInterfaceWidget = true;
	statusBar.isInterfaceWidget = true;

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

void AppInterface::HandleClick(int mouseX, int mouseY)
{
	if (activeWidget)
	{
		activeWidget = NULL;
	}

	if (hoverWidget)
	{
		if (hoverWidget->GetLinkURL())
		{
			app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
		}
		else if (hoverWidget->type == Widget::TextField)
		{
			activeWidget = hoverWidget;
		}
		else if (hoverWidget->type == Widget::Button)
		{
			activeWidget = hoverWidget;
			app.renderer.InvertWidget(activeWidget);
		}
		else if (hoverWidget->type == Widget::ScrollBar)
		{
			//activeWidget = hoverWidget;
			int relPos = mouseY - hoverWidget->y;

			if (relPos < hoverWidget->scrollBar->position)
			{
				app.renderer.Scroll(-(Platform::video->windowHeight - 24));
			}
			else if (relPos > hoverWidget->scrollBar->position + hoverWidget->scrollBar->size)
			{
				app.renderer.Scroll(Platform::video->windowHeight - 24);
			}
			else
			{
				activeWidget = hoverWidget;
				scrollBarRelativeClickPositionX = mouseX - hoverWidget->x;
				scrollBarRelativeClickPositionY = relPos - hoverWidget->scrollBar->position;
			}
		}
	}
}

void AppInterface::HandleRelease()
{
	if (activeWidget && activeWidget->type == Widget::Button)
	{
		app.renderer.InvertWidget(activeWidget);

		if (hoverWidget == activeWidget)
		{
			if (activeWidget->button->form)
			{
				SubmitForm(activeWidget->button->form);
			}
			else if (activeWidget == &backButton)
			{
				app.PreviousPage();
			}
			else if (activeWidget == &forwardButton)
			{
				app.NextPage();
			}
		}
		activeWidget = NULL;
	}
	else if (activeWidget == &scrollBar)
	{
		if (scrollBar.scrollBar->size < scrollBar.height)
		{
			int targetScroll = ((int32_t) app.renderer.GetMaxScrollPosition() * scrollBar.scrollBar->position) / (scrollBar.height - scrollBar.scrollBar->size);
			app.renderer.ScrollTo(targetScroll);
		}
		activeWidget = NULL;
	}
}

bool AppInterface::HandleActiveWidget(InputButtonCode keyPress)
{
	switch (activeWidget->type)
	{
	case Widget::TextField:
		{
			if(activeWidget->textField && activeWidget->textField->buffer)
			{
				int textLength = strlen(activeWidget->textField->buffer);

				if (keyPress >= 32 && keyPress < 128)
				{
					if (textLength < activeWidget->textField->bufferLength)
					{
						app.renderer.RenderWidget(activeWidget);
						activeWidget->textField->buffer[textLength++] = (char)(keyPress);
						activeWidget->textField->buffer[textLength++] = '\0';
						app.renderer.RenderWidget(activeWidget);
					}
					return true;
				}
				else if (keyPress == KEYCODE_BACKSPACE)
				{
					if (textLength > 0)
					{
						app.renderer.RenderWidget(activeWidget);
						activeWidget->textField->buffer[textLength - 1] = '\0';
						app.renderer.RenderWidget(activeWidget);
					}
					return true;
				}
				else if (keyPress == KEYCODE_ENTER)
				{
					if (activeWidget == &addressBar)
					{
						app.OpenURL(addressBarURL.url);
					}
					else if (activeWidget->textField->form)
					{
						SubmitForm(activeWidget->textField->form);
					}
				}
			}
		}
		break;
	}

	return false;
}

void AppInterface::SubmitForm(WidgetFormData* form)
{
	if (form->method == WidgetFormData::Get)
	{
		char* address = addressBarURL.url;
		strcpy(address, form->action);
		int numParams = 0;

		for (int n = 0; n < app.page.numFinishedWidgets; n++)
		{
			Widget& widget = app.page.widgets[n];

			switch (widget.type)
			{
			case Widget::TextField:
				if (widget.textField->form == form && widget.textField->name && widget.textField->buffer)
				{
					if (numParams == 0)
					{
						strcat(address, "?");
					}
					else
					{
						strcat(address, "&");
					}
					strcat(address, widget.textField->name);
					strcat(address, "=");
					strcat(address, widget.textField->buffer);
					numParams++;
				}
				break;
			}
		}

		// Replace any spaces with +
		for (char* p = address; *p; p++)
		{
			if (*p == ' ')
			{
				*p = '+';
			}
		}

		app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, address).url);
	}
}

void AppInterface::UpdateAddressBar(const URL& url)
{
	addressBarURL = url;
	Platform::video->ClearRect(addressBar.x + 1, addressBar.y + 1, addressBar.width - 2, addressBar.height - 2);
	app.renderer.RenderWidget(&addressBar);
}

void AppInterface::UpdatePageScrollBar()
{
	int pageHeight = app.page.GetPageHeight();
	int windowHeight = Platform::video->windowHeight;
	int scrollWidgetSize = pageHeight < windowHeight ? windowHeight : windowHeight * windowHeight / pageHeight;
	if (scrollWidgetSize < MIN_SCROLL_WIDGET_SIZE)
		scrollWidgetSize = MIN_SCROLL_WIDGET_SIZE;
	int maxWidgetPosition = windowHeight - scrollWidgetSize;

	int maxScroll = app.renderer.GetMaxScrollPosition();
	int widgetPosition = maxScroll == 0 ? 0 : (int32_t)maxWidgetPosition * app.renderer.GetScrollPosition() / maxScroll;

	scrollBar.scrollBar->size = scrollWidgetSize;

	if (activeWidget != &scrollBar)
	{
		scrollBar.scrollBar->position = widgetPosition;
	}

	app.renderer.RedrawScrollBar();
}
