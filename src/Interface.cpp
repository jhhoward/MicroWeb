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
	textFieldCursorPosition = 0;
	clickingButton = false;
}

void AppInterface::Reset()
{
	oldPageHeight = 0;
	if (activeWidget && !activeWidget->isInterfaceWidget)
	{
		activeWidget = NULL;
	}
	if (hoverWidget && !hoverWidget->isInterfaceWidget)
	{
		hoverWidget = NULL;
	}
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
		case KEYCODE_BACKSPACE:
			app.PreviousPage();
			break;
		case KEYCODE_F2:
		{
			Platform::input->HideMouse();
			Platform::video->InvertScreen();
			Platform::input->ShowMouse();
		}
			break;
		case KEYCODE_CTRL_L:
		case KEYCODE_F6:
			ActivateWidget(&addressBar);
			break;

		case KEYCODE_TAB:
			CycleWidgets(1);
			break;
		case KEYCODE_SHIFT_TAB:
			CycleWidgets(-1);
			break;

		default:
//			printf("%x\n", keyPress);
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

void AppInterface::DeactivateWidget()
{
	if (activeWidget)
	{
		if (activeWidget->type == Widget::Button)
		{
			if (clickingButton)
			{
				app.renderer.InvertWidget(activeWidget);
			}
			else
			{
				Widget* widget = activeWidget;
				activeWidget = NULL;
				app.renderer.RedrawWidget(widget);
			}
		}
		if (activeWidget->type == Widget::TextField)
		{
			if (textFieldCursorPosition == -1)
			{
				app.renderer.InvertWidget(activeWidget);
			}
			else
			{
				app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, true);
			}
		}
		else if (activeWidget->GetLinkURL())
		{
			InvertWidgetsWithLinkURL(activeWidget->GetLinkURL());
		}
		activeWidget = NULL;
	}
}

void AppInterface::ActivateWidget(Widget* widget)
{
	if (activeWidget && activeWidget != widget)
	{
		DeactivateWidget();
	}

	activeWidget = widget;

	if (activeWidget)
	{
		switch (activeWidget->type)
		{
		case Widget::Button:
			if (clickingButton)
			{
				app.renderer.InvertWidget(activeWidget);
			}
			else
			{
				app.renderer.RedrawWidget(activeWidget);
			}
			break;
		case Widget::TextField:
			if (activeWidget == &addressBar && strlen(activeWidget->textField->buffer) > 0)
			{
				textFieldCursorPosition = -1;
				app.renderer.InvertWidget(activeWidget);
			}
			else
			{
				textFieldCursorPosition = strlen(activeWidget->textField->buffer);
				app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
			}
			break;
		default:
			if (widget->GetLinkURL())
			{
				InvertWidgetsWithLinkURL(widget->GetLinkURL());
				app.renderer.SetStatus(URL::GenerateFromRelative(app.page.pageURL.url, widget->GetLinkURL()).url);
			}
			break;
		}
	}
}

void AppInterface::InvertWidgetsWithLinkURL(const char* url)
{
	for (int n = app.renderer.GetPageTopWidgetIndex(); n < app.page.numFinishedWidgets; n++)
	{
		Widget* pageWidget = &app.page.widgets[n];
		if (pageWidget->y > app.renderer.GetScrollPosition() + Platform::video->windowHeight)
		{
			break;
		}
		if (pageWidget->GetLinkURL() == url)
		{
			app.renderer.InvertWidget(pageWidget);
		}
	}
}

void AppInterface::HandleClick(int mouseX, int mouseY)
{
	if (activeWidget && activeWidget != hoverWidget)
	{
		DeactivateWidget();
	}

	if (hoverWidget)
	{
		if (hoverWidget->GetLinkURL())
		{
			app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, hoverWidget->GetLinkURL()).url);
		}
		else if (hoverWidget->type == Widget::TextField)
		{
			bool shouldPick = hoverWidget != &addressBar || hoverWidget == activeWidget;
			if (hoverWidget != activeWidget)
			{
				ActivateWidget(hoverWidget);
			}

			if(shouldPick)
			{
				if (hoverWidget == activeWidget && textFieldCursorPosition == -1)
				{
					app.renderer.InvertWidget(activeWidget);
				}
				activeWidget = hoverWidget;

				int pickX = activeWidget->x + 3;
				int pickIndex = 0;
				for (char* str = activeWidget->textField->buffer; *str; str++, pickIndex++)
				{
					if (mouseX < pickX)
					{
						break;
					}
					pickX += Platform::video->GetGlyphWidth(*str);
				}

				app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, true);
				textFieldCursorPosition = pickIndex;
				app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
			}
		}
		else if (hoverWidget->type == Widget::Button)
		{
			clickingButton = true;
			ActivateWidget(hoverWidget);
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

void AppInterface::HandleButtonClicked(Widget* widget)
{
	if(widget->type == Widget::Button)
	{
		if (widget->button->form)
		{
			SubmitForm(widget->button->form);
		}
		else if (widget == &backButton)
		{
			app.PreviousPage();
		}
		else if (widget == &forwardButton)
		{
			app.NextPage();
		}
	}
}

void AppInterface::HandleRelease()
{
	if (activeWidget && activeWidget->type == Widget::Button)
	{
		if (clickingButton)
		{
			if (hoverWidget == activeWidget)
			{
				HandleButtonClicked(activeWidget);
			}
			DeactivateWidget();

			activeWidget = NULL;
			clickingButton = false;
		}
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
				if (keyPress >= 32 && keyPress < 128)
				{
					if (textFieldCursorPosition == -1)
					{
						activeWidget->textField->buffer[0] = '\0';
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = 0;
					}

					if (textFieldCursorPosition < activeWidget->textField->bufferLength)
					{
						int len = strlen(activeWidget->textField->buffer);
						for (int n = len; n >= textFieldCursorPosition; n--)
						{
							activeWidget->textField->buffer[n + 1] = activeWidget->textField->buffer[n];
						}
						activeWidget->textField->buffer[textFieldCursorPosition++] = (char)(keyPress);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition - 1, true);
						app.renderer.RedrawModifiedTextField(activeWidget, textFieldCursorPosition - 1);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					return true;
				}
				else if (keyPress == KEYCODE_BACKSPACE)
				{
					if (textFieldCursorPosition == -1)
					{
						activeWidget->textField->buffer[0] = '\0';
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = 0;
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					else if (textFieldCursorPosition > 0)
					{
						int len = strlen(activeWidget->textField->buffer);
						for (int n = textFieldCursorPosition - 1; n < len; n++)
						{
							activeWidget->textField->buffer[n] = activeWidget->textField->buffer[n + 1];
						}						
						textFieldCursorPosition--;
						app.renderer.RedrawModifiedTextField(activeWidget, textFieldCursorPosition);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					return true;
				}
				else if (keyPress == KEYCODE_DELETE)
				{
					int len = strlen(activeWidget->textField->buffer);
					if (textFieldCursorPosition == -1)
					{
						activeWidget->textField->buffer[0] = '\0';
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = 0;
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					else if (textFieldCursorPosition < len)
					{
						for (int n = textFieldCursorPosition; n < len; n++)
						{
							activeWidget->textField->buffer[n] = activeWidget->textField->buffer[n + 1];
						}
						app.renderer.RedrawModifiedTextField(activeWidget, textFieldCursorPosition);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
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
					DeactivateWidget();
					return true;
				}
				else if (keyPress == KEYCODE_ARROW_LEFT)
				{
					if (textFieldCursorPosition == -1)
					{
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = strlen(activeWidget->textField->buffer);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					else if (textFieldCursorPosition > 0)
					{
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, true);
						textFieldCursorPosition--;
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					return true;
				}
				else if (keyPress == KEYCODE_ARROW_RIGHT)
				{
					if (textFieldCursorPosition == -1)
					{
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = strlen(activeWidget->textField->buffer);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					else if (textFieldCursorPosition < strlen(activeWidget->textField->buffer))
					{
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, true);
						textFieldCursorPosition++;
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					return true;
				}
				else if (keyPress == KEYCODE_END)
				{
					if (textFieldCursorPosition == -1)
					{
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = strlen(activeWidget->textField->buffer);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					else
					{
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, true);
						textFieldCursorPosition = strlen(activeWidget->textField->buffer);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					return true;
				}
				else if (keyPress == KEYCODE_HOME)
				{
					if (textFieldCursorPosition == -1)
					{
						app.renderer.RedrawWidget(activeWidget);
						textFieldCursorPosition = strlen(activeWidget->textField->buffer);
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					else
					{
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, true);
						textFieldCursorPosition = 0;
						app.renderer.DrawTextFieldCursor(activeWidget, textFieldCursorPosition, false);
					}
					return true;
				}
			}
		}
		break;
	case Widget::Button:
		if (clickingButton)
		{
			return true;
		}
		if (keyPress == KEYCODE_ENTER)
		{
			HandleButtonClicked(activeWidget);
		}
		break;
	default:
		if (keyPress == KEYCODE_ENTER && activeWidget->GetLinkURL())
		{
			app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, activeWidget->GetLinkURL()).url);
			return true;
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
	app.renderer.RedrawWidget(&addressBar);
}

void AppInterface::UpdatePageScrollBar()
{
	int pageHeight = app.page.GetPageHeight();
	int windowHeight = Platform::video->windowHeight;
	int scrollWidgetSize = pageHeight < windowHeight ? windowHeight : (int)(((int32_t)windowHeight * windowHeight) / pageHeight);
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

void AppInterface::CycleWidgets(int direction)
{
	if (app.page.numFinishedWidgets == 0)
	{
		return;
	}

	int pageScrollBottom = app.renderer.GetScrollPosition() + Platform::video->windowHeight;

	// If there is an active widget, find index and check if it is on screen
	int currentPos = -1;
	if (activeWidget && !activeWidget->isInterfaceWidget)
	{
		for (int n = 0; n < app.page.numFinishedWidgets; n++)
		{
			Widget* widget = &app.page.widgets[n];
			if (activeWidget == widget)
			{
				if (app.renderer.GetScrollPosition() < widget->y + widget->height && pageScrollBottom > widget->y)
				{
					currentPos = n;
				}
				break;
			}
		}
	}

	// If there is no active widget or not on screen, find the first widget on screen
	if (currentPos == -1)
	{
		if (direction > 0)
		{
			for (currentPos = app.renderer.GetPageTopWidgetIndex(); currentPos < app.page.numFinishedWidgets; currentPos++)
			{
				Widget* widget = &app.page.widgets[currentPos];
				if (widget->y >= app.renderer.GetScrollPosition())
				{
					break;
				}
			}

			if (currentPos == app.page.numFinishedWidgets)
			{
				currentPos = 0;
			}
		}
		else
		{
			for (currentPos = app.page.numFinishedWidgets - 1; currentPos >= 0; currentPos--)
			{
				Widget* widget = &app.page.widgets[currentPos];
				if (widget->y + widget->height < pageScrollBottom)
				{
					break;
				}
			}

			if (currentPos < 0)
			{
				currentPos = app.page.numFinishedWidgets - 1;
			}
		}
	}

	int startPos = currentPos;

	while (1)
	{
		Widget* widget = &app.page.widgets[currentPos];
		if (widget != activeWidget)
		{
			if ((widget->type == Widget::TextField) || (widget->type == Widget::Button) || (widget->GetLinkURL() && widget->GetLinkURL() != activeWidget->GetLinkURL()))
			{
				ActivateWidget(widget);
				app.renderer.ScrollTo(widget);
				return;
			}
		}

		currentPos += direction;
		if (currentPos >= app.page.numFinishedWidgets)
		{
			currentPos = 0;
		}
		if (currentPos < 0)
		{
			currentPos = app.page.numFinishedWidgets - 1;
		}
		if (currentPos == startPos)
		{
			break;
		}
	}
}
