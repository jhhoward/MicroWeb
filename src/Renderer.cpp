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
	Platform::video->ClearWindow();
	RedrawScrollBar();
	app.ui.DrawInterfaceWidgets();
	SetTitle("MicroWeb");
	SetStatus("");
	Platform::input->ShowMouse();
}

void Renderer::Reset()
{
	pageTopWidgetIndex = 0;
	oldStatus = NULL;
	scrollPosition = 0;
	upperRenderLine = Platform::video->windowY;
	lowerRenderLine = Platform::video->windowY;
	Platform::input->HideMouse();
	Platform::video->ClearWindow();
	Platform::input->ShowMouse();
}

void Renderer::InvertWidget(Widget* widget)
{
	int baseY;
	BeginWidgetDraw(widget, baseY);

	int x = widget->x;
	int y = widget->y + baseY;
	int width = widget->width;
	int height = widget->height;

	if (widget->type == Widget::Button)
	{
		x++;
		y++;
		width -= 2;
		height -= 2;
	}

	if (widget->type == Widget::TextField)
	{
		if (widget->textField && widget->textField->buffer)
		{
			x += 2;
			y += 2;
			width = Platform::video->GetFont(1)->CalculateWidth(widget->textField->buffer);
			height = Platform::video->GetFont(1)->glyphHeight;
		}
	}

	Platform::video->InvertRect(x, y, width, height);
	EndWidgetDraw();
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
					RenderWidgetInternal(widget, baseY);
					line = widgetLine;
					renderedAnything = true;
				}
				else
				{
					break;
				}
			}
		}

		Platform::video->ClearScissorRegion();

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

			Platform::video->SetScissorRegion(Platform::video->windowY, upperRenderLine);

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
				RenderWidgetInternal(widget, baseY);
				if (widgetTop < upperRenderLine)
				{
					upperRenderLine = widgetTop;
				}
			}

			Platform::video->ClearScissorRegion();

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

void Renderer::ScrollTo(int targetPosition)
{
	Scroll(targetPosition - scrollPosition);
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
	}
	else
	{
		Platform::video->ClearWindow();
	}

	scrollPosition += delta;

	app.ui.UpdatePageScrollBar();

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
	Platform::input->HideMouse();
	Platform::video->DrawScrollBar(app.ui.scrollBar.scrollBar->position, app.ui.scrollBar.scrollBar->size);
	Platform::input->ShowMouse();
}

void Renderer::RenderWidgetInternal(Widget* widget, int baseY)
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
				int textWidth = 0;
				int maxTextWidth = widget->width - 4;
				char* replaceChar = NULL;
				char oldChar = 0;

				for (char* p = widget->textField->buffer; *p; p++)
				{
					textWidth += Platform::video->GetGlyphWidth(*p);
					if (textWidth > maxTextWidth)
					{
						oldChar = *p;
						replaceChar = p;
						*p = '\0';
						break;
					}
				}
				Platform::video->DrawString(widget->textField->buffer, widget->x + 3, widget->y + baseY + 2);

				if (replaceChar)
				{
					*replaceChar = oldChar;
				}
			}
		}
		break;
	}
}

void Renderer::RedrawModifiedTextField(Widget* widget, int position)
{
	int baseY = 0;

	int x = widget->x + 3;
	int width = widget->width - 4;
	int maxX = widget->x + width - 1;
	char* str = widget->textField->buffer;
	char* lastChar = NULL;
	char oldChar = 0;

	for (int n = 0; n < position && *str; n++, str++)
	{
		int glyphWidth = Platform::video->GetGlyphWidth(widget->textField->buffer[n]);
		width -= glyphWidth;
		x += glyphWidth;
		if (width <= 0)
		{
			break;
		}
		if (x >= maxX)
		{
			lastChar = str;
			oldChar = *lastChar;
			*lastChar = '\0';
		}
	}

	if (width > 0)
	{
		BeginWidgetDraw(widget, baseY);
		int y = widget->y + 2 + baseY;

		Platform::video->ClearRect(x, y, width, Platform::video->GetFont(1)->glyphHeight);
		Platform::video->DrawString(str, x, y);

		Platform::video->ClearScissorRegion();
	}

	if (lastChar)
	{
		*lastChar = oldChar;
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

bool Renderer::IsOverWidget(Widget* widget, int x, int y)
{
	if (widget->isInterfaceWidget)
	{
		return x >= widget->x && y >= widget->y && x < widget->x + widget->width && y < widget->y + widget->height;
	}

	int adjustY = scrollPosition - Platform::video->windowY;

	return (x >= widget->x && y >= widget->y + adjustY && x < widget->x + widget->width && y < widget->y + widget->height + adjustY);
}

void Renderer::SetTitle(const char* title)
{
	Platform::input->HideMouse();

	Platform::video->FillRect(app.ui.titleBar.x, app.ui.titleBar.y, app.ui.titleBar.width, app.ui.titleBar.height);
	if (title)
	{
		int textWidth = Platform::video->GetFont(1)->CalculateWidth(title);
		Platform::video->DrawString(title, app.ui.titleBar.x + Platform::video->screenWidth / 2 - textWidth / 2, app.ui.titleBar.y, 1);
	}

	Platform::input->ShowMouse();
}

void Renderer::SetStatus(const char* status)
{
	if (status != oldStatus)
	{
		Platform::input->HideMouse();

		Platform::video->FillRect(app.ui.statusBar.x, app.ui.statusBar.y, app.ui.statusBar.width, app.ui.statusBar.height);
		if (status)
		{
			Platform::video->DrawString(status, app.ui.statusBar.x, app.ui.statusBar.y, 1);
		}

		Platform::input->ShowMouse();
		oldStatus = status;
	}
}

void Renderer::DrawButtonRect(int x, int y, int width, int height)
{
	Platform::video->HLine(x + 1, y, width - 2);
	Platform::video->HLine(x + 1, y + height - 1, width - 2);
	Platform::video->VLine(x, y + 1, height - 2);
	Platform::video->VLine(x + width - 1, y + 1, height - 2);
}

void Renderer::RenderWidget(Widget* widget)
{
	int baseY;
	BeginWidgetDraw(widget, baseY);
	RenderWidgetInternal(widget, baseY);
	EndWidgetDraw();
}

void Renderer::RedrawWidget(Widget* widget)
{
	int baseY;
	BeginWidgetDraw(widget, baseY);

	Platform::video->ClearRect(widget->x, widget->y + baseY, widget->width, widget->height);
	RenderWidgetInternal(widget, baseY);

	EndWidgetDraw();
}

void Renderer::DrawTextFieldCursor(Widget* widget, int position, bool clear)
{
	if (!widget || !widget->textField || !widget->textField->buffer)
		return;


	int x = widget->x + 2;
	int height = Platform::video->GetFont(1)->glyphHeight;

	for (int n = 0; n < position; n++)
	{
		if (!widget->textField->buffer[n])
			break;
		x += Platform::video->GetGlyphWidth(widget->textField->buffer[n]);
	}

	if (x >= widget->x + widget->width - 1)
	{
		return;
	}

	int baseY;
	BeginWidgetDraw(widget, baseY);
	int y = widget->y + 2 + baseY;

	if (clear)
	{
		Platform::video->ClearRect(x, y, 1, height);
	}
	else
	{
		Platform::video->VLine(x, y, height);
	}

	EndWidgetDraw();
}

void Renderer::BeginWidgetDraw(Widget* widget, int& baseY)
{
	Platform::input->HideMouse();

	if (!widget->isInterfaceWidget)
	{
		baseY = Platform::video->windowY - scrollPosition;
	}
	else baseY = 0;
}

void Renderer::EndWidgetDraw()
{
	Platform::video->ClearScissorRegion();
	Platform::input->ShowMouse();
}
