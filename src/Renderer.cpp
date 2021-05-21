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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "Renderer.h"
#include "Widget.h"
#include "Platform.h"
#include "App.h"

#define MIN_SCROLL_WIDGET_SIZE 8

Renderer::Renderer(App& inApp) 
	: app(inApp)
{
	pageTopWidgetIndex = 0;
	oldStatus = NULL;
	scrollPosition = 0;
}

void Renderer::Init()
{
	Platform::input->HideMouse();
	Platform::video->SetScissorRegion(0, Platform::video->screenHeight);
	Platform::video->ClearWindow();
	RedrawScrollBar();
	app.ui.DrawInterfaceWidgets();
	Platform::input->ShowMouse();
}

void Renderer::Reset()
{
	pageTopWidgetIndex = 0;
	oldStatus = NULL;
	scrollPosition = 0;
	upperRenderLine = Platform::video->windowY;
	lowerRenderLine = Platform::video->windowY;
	oldPageHeight = 0;
	Platform::input->HideMouse();
	Platform::video->ClearWindow();
	RedrawScrollBar();
	Platform::input->ShowMouse();
}

int Renderer::GetMaxScrollPosition()
{
	int maxScroll = app.page.GetPageHeight() - Platform::video->windowHeight;
	if (maxScroll < 0)
	{
		maxScroll = 0;
	}
	return maxScroll;
}

void Renderer::Update()
{
	if (app.page.GetPageHeight() != oldPageHeight)
	{
		oldPageHeight = app.page.GetPageHeight();
		RedrawScrollBar();
	}

	int baseY = Platform::video->windowY - scrollPosition;
	int lowerWindowY = Platform::video->windowY + Platform::video->windowHeight;
	bool renderedAnything = false;

	if (lowerRenderLine < lowerWindowY)
	{
		int line = -1;

		Platform::video->SetScissorRegion(lowerRenderLine, lowerWindowY);

		for (int n = pageTopWidgetIndex; n < app.page.numFinishedWidgets; n++)
		{
			Widget* widget = &app.page.widgets[n];
			int widgetLine = widget->y + widget->height + baseY;

			if (widgetLine > lowerRenderLine)
			{
				if (line == -1 || widgetLine == line)
				{
					Platform::input->HideMouse();
					RenderWidget(widget, baseY);
					line = widgetLine;
					renderedAnything = true;
				}
				else
				{
					break;
				}
			}
		}

		if (line != -1)
		{
			lowerRenderLine = line;
			if (lowerRenderLine > lowerWindowY)
			{
				lowerRenderLine = lowerWindowY;
			}
		}
	}

	if (renderedAnything)
	{
		Platform::input->ShowMouse();
		return;
	}

	if (upperRenderLine > Platform::video->windowY)
	{
		Platform::video->SetScissorRegion(Platform::video->windowY, upperRenderLine);

		int bestLineStartIndex = -1;
		int currentLineIndex = -1;
		int currentLineHeight = -1;
		int currentLineTop = -1;

		for (int n = pageTopWidgetIndex; n < app.page.numFinishedWidgets; n++)
		{
			Widget* widget = &app.page.widgets[n];
			int widgetTop = widget->y + baseY;
			int widgetLine = widgetTop + widget->height;

			if (currentLineIndex == -1 || widgetLine != currentLineHeight)
			{
				// First widget in the line - possibly a new line

				if (currentLineTop != -1)
				{
					if (currentLineTop < upperRenderLine)
					{
						bestLineStartIndex = currentLineIndex;
					}
					else
					{
						// Last line was completely below the cutoff line so stop
						break;
					}
				}

				currentLineIndex = n;
				currentLineHeight = widgetLine;
				currentLineTop = widgetTop;
			}

			if (widgetTop < currentLineTop)
			{
				currentLineTop = widgetTop;
			}
		}

		if (bestLineStartIndex != -1)
		{
			int currentLineHeight = -1;

			for (int n = bestLineStartIndex; n < app.page.numFinishedWidgets; n++)
			{
				Widget* widget = &app.page.widgets[n];
				int widgetTop = widget->y + baseY;
				int widgetLine = widgetTop + widget->height;

				if (currentLineHeight == -1)
				{
					currentLineHeight = widgetLine;
				}
				if (widgetLine != currentLineHeight)
				{
					break;
				}

				Platform::input->HideMouse();
				RenderWidget(widget, baseY);
				if (widgetTop < upperRenderLine)
				{
					upperRenderLine = widgetTop;
				}
			}

			if (upperRenderLine < Platform::video->windowY)
			{
				upperRenderLine = Platform::video->windowY;
			}
		}
		else
		{
			// Everything must have been rendered, reset 
			upperRenderLine = Platform::video->windowY;
		}
	}

	Platform::input->ShowMouse();
}

void Renderer::Scroll(int delta)
{
	if (scrollPosition + delta < 0)
	{
		delta = -scrollPosition;
	}
	int maxScroll = GetMaxScrollPosition();
	if (scrollPosition + delta > maxScroll)
	{
		delta = maxScroll - scrollPosition;
	}

	delta &= ~1;

	if (delta == 0)
	{
		return;
	}

	Platform::input->HideMouse();

	if (delta < Platform::video->windowHeight && delta > -Platform::video->windowHeight)
	{
		Platform::video->ScrollWindow(delta);

		if (delta > 0)
		{
			Platform::video->SetScissorRegion(Platform::video->windowY + Platform::video->windowHeight - delta, Platform::video->windowY + Platform::video->windowHeight);
		}
		else
		{
			Platform::video->SetScissorRegion(Platform::video->windowY, Platform::video->windowY - delta);
		}
	}
	else
	{
		Platform::video->SetScissorRegion(Platform::video->windowY, Platform::video->windowY + Platform::video->windowHeight);
		Platform::video->ClearWindow();
	}

	scrollPosition += delta;

	RedrawScrollBar();

	if (delta < 0)
	{
		for (int n = pageTopWidgetIndex - 1; n >= 0; n--)
		{
			Widget* widget = &app.page.widgets[n];
			if (widget->y + widget->height < scrollPosition)
			{
				break;
			}
			pageTopWidgetIndex = n;
		}
	}
	else
	{
		for (int n = pageTopWidgetIndex; n < app.page.numFinishedWidgets; n++)
		{
			Widget* widget = &app.page.widgets[n];
			if (widget->y + widget->height < scrollPosition && n == pageTopWidgetIndex)
			{
				pageTopWidgetIndex++;
			}
			else break;
		}
	}

	lowerRenderLine -= delta;
	upperRenderLine -= delta;
	int windowBottom = Platform::video->windowY + Platform::video->windowHeight;

	if (lowerRenderLine < Platform::video->windowY)
	{
		lowerRenderLine = Platform::video->windowY;
	}
	if (lowerRenderLine > windowBottom)
	{
		lowerRenderLine = windowBottom;
	}
	if (upperRenderLine < Platform::video->windowY)
	{
		upperRenderLine = Platform::video->windowY;
	}
	if (upperRenderLine > windowBottom)
	{
		upperRenderLine = windowBottom;
	}

	Platform::input->ShowMouse();
}

void Renderer::RedrawScrollBar()
{
	int pageHeight = app.page.GetPageHeight();
	int windowHeight = Platform::video->windowHeight;
	int scrollWidgetSize = pageHeight < windowHeight ? windowHeight : windowHeight * windowHeight / pageHeight;
	if (scrollWidgetSize < MIN_SCROLL_WIDGET_SIZE)
		scrollWidgetSize = MIN_SCROLL_WIDGET_SIZE;
	int maxWidgetPosition = windowHeight - scrollWidgetSize;

	int maxScroll = GetMaxScrollPosition();
	int widgetPosition = maxScroll == 0 ? 0 : (int32_t)maxWidgetPosition * scrollPosition / maxScroll;

	Platform::video->DrawScrollBar(widgetPosition, scrollWidgetSize);
}

void Renderer::RenderWidget(Widget* widget, int baseY)
{
	switch (widget->type)
	{
	case Widget::Text:
		if (widget->text->text)
		{
			Platform::video->DrawString(widget->text->text, widget->x, widget->y + baseY, widget->style.fontSize, widget->style.fontStyle);
		}
		break;
	case Widget::HorizontalRule:
		Platform::video->HLine(widget->x, widget->y + baseY, widget->width);
		break;
	case Widget::Button:
		if (widget->button)
		{
			DrawButtonRect(widget->x, widget->y + baseY, widget->width, widget->height);
			Platform::video->DrawString(widget->button->text, widget->x + 8, widget->y + baseY + 2, widget->style.fontSize, widget->style.fontStyle);
		}
		break;
	case Widget::TextField:
		if (widget->textField)
		{
			DrawButtonRect(widget->x, widget->y + baseY, widget->width, widget->height);
			if (widget->textField->buffer)
			{
				Platform::video->DrawString(widget->textField->buffer, widget->x + 2, widget->y + baseY + 2, widget->style.fontSize, widget->style.fontStyle);
			}
		}
		break;
	}
}

Widget* Renderer::PickPageWidget(int x, int y)
{
	if (y < upperRenderLine || y > lowerRenderLine || x > Platform::video->windowWidth)
	{
		return NULL;
	}
	return app.page.GetWidget(x, y - Platform::video->windowY + scrollPosition);
}

bool Renderer::IsOverPageWidget(Widget* widget, int x, int y)
{
	int adjustY = scrollPosition - Platform::video->windowY;

	return (x >= widget->x && y >= widget->y + adjustY && x < widget->x + widget->width && y < widget->y + widget->height + adjustY);
}

void Renderer::DrawStatus(const char* status)
{
	if (status != oldStatus)
	{
		Platform::input->HideMouse();

		Platform::video->DrawStatus(status ? status : "");

		Platform::input->ShowMouse();
		oldStatus = status;
	}
}

void Renderer::DrawAddress(const char* address)
{
	Platform::input->HideMouse();

	Platform::video->SetScissorRegion(0, Platform::video->windowY);
	Widget& addressBar = app.ui.addressBar;

	Platform::video->ClearRect(addressBar.x + 1, addressBar.y + 1, addressBar.width - 2, addressBar.height - 2);
	Platform::video->DrawString(address, addressBar.x + 2, addressBar.y + 2);

	Platform::input->ShowMouse();

}

void Renderer::DrawButtonRect(int x, int y, int width, int height)
{
	Platform::video->HLine(x + 1, y, width - 2);
	Platform::video->HLine(x + 1, y + height - 1, width - 2);
	Platform::video->VLine(x, y + 1, height - 2);
	Platform::video->VLine(x + width - 1, y + 1, height - 2);
}

void Renderer::RenderPageWidget(Widget* widget)
{
	Platform::video->SetScissorRegion(upperRenderLine, lowerRenderLine);
	int baseY = Platform::video->windowY - scrollPosition;

	RenderWidget(widget, baseY);
}
