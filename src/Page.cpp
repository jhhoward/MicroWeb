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
#include <string.h>
#include "Platform.h"
#include "Page.h"
#include "App.h"

#define TOP_MARGIN_PADDING 1

Page::Page(App& inApp) : app(inApp)
{
	Reset();
}

void Page::Reset()
{
	currentLineStartWidgetIndex = -1;
	currentWidgetIndex = -1;
	numFinishedWidgets = 0;
	pageWidth = Platform::video->windowWidth;
	pageHeight = 0;
	numWidgets = 0;
	needLeadingWhiteSpace = false;
	pendingVerticalPadding = 0;
	widgetURL = NULL;
	textBufferSize = 0;
	leftMarginPadding = 1;
	cursorX = leftMarginPadding;
	cursorY = TOP_MARGIN_PADDING;
	formData = NULL;

	styleStackSize = 1;
	styleStack[0] = WidgetStyle(FontStyle::Regular, 1, false);
	allocator.Reset();
}

WidgetStyle& Page::GetStyleStackTop()
{
	return styleStack[styleStackSize - 1];
}

void Page::PushStyle(const WidgetStyle& style)
{
	if (styleStackSize < MAX_PAGE_STYLE_STACK_SIZE)
	{
		styleStack[styleStackSize] = style;
		styleStackSize++;
		FinishCurrentWidget();
	}
}

void Page::PopStyle()
{
	if (styleStackSize > 1)
	{
		styleStackSize--;
	}
	FinishCurrentWidget();
}

void Page::AddHorizontalRule()
{
	BreakLine(1);
	Widget* widget = CreateWidget(Widget::HorizontalRule);
	if (widget)
	{
		widget->width = Platform::video->windowWidth - 8;
		widget->x = 4;
		widget->height = 1;
	}
	BreakLine(1);
}

void Page::AddButton(char* text)
{
	Widget* widget = CreateWidget(Widget::Button);
	if (widget)
	{
		widget->button = allocator.Alloc<ButtonWidgetData>();
		if (widget->button)
		{
			widget->button->form = formData;
			widget->button->text = text;
			widget->height = Platform::video->GetFont(1)->glyphHeight + 4;
			widget->width = Platform::video->GetFont(1)->CalculateWidth(text) + 16;
		}
	}
}

void Page::AddTextField(char* text, int bufferLength, char* name)
{
	Widget* widget = CreateWidget(Widget::TextField);
	if (widget)
	{
		widget->textField = allocator.Alloc<TextFieldWidgetData>();
		if (widget->textField)
		{
			widget->textField->form = formData;
			widget->textField->name = name;
			widget->textField->buffer = NULL;
			if (text)
			{
				int textLength = strlen(text);
				if (textLength > bufferLength)
				{
					bufferLength = textLength;
				}
			}
			widget->textField->buffer = (char*) allocator.Alloc(bufferLength + 1);
			if (widget->textField->buffer)
			{
				if (text)
				{
					strcpy(widget->textField->buffer, text);
				}
				else
				{
					widget->textField->buffer[0] = '\0';
				}
			}
			widget->textField->bufferLength = bufferLength;

			widget->width = 200;
			widget->height = Platform::video->GetFont(1)->glyphHeight + 4;
			//widget->width = Platform::video->GetFont(1)->CalculateWidth(text) + 4;
		}
	}
}

Widget* Page::CreateTextWidget()
{
	Widget* widget = CreateWidget(Widget::Text);
	if (widget)
	{
		widget->text = allocator.Alloc<TextWidgetData>();
		if (widget->text)
		{
			widget->text->linkURL = widgetURL;
			widget->height = Platform::video->GetLineHeight(widget->style.fontSize);
			widget->text->text = NULL;
		}
	}
	return widget;
}

Widget* Page::CreateWidget(Widget::Type type)
{
	if (currentWidgetIndex != -1)
	{
		FinishCurrentWidget();
	}

	if (numWidgets < MAX_PAGE_WIDGETS)
	{
		currentWidgetIndex = numWidgets;
		numWidgets++;

		Widget& current = widgets[currentWidgetIndex];
		current.type = type;

		if (needLeadingWhiteSpace)
		{
			if (cursorX > leftMarginPadding)
			{
				cursorX += Platform::video->GetGlyphWidth(' ');
			}
			needLeadingWhiteSpace = false;
		}

		cursorY += pendingVerticalPadding;
		pendingVerticalPadding = 0;

		current.style = GetStyleStackTop();
		current.x = cursorX;
		current.y = cursorY;
		current.width = 0;
		current.height = 0;
		current.text = NULL;

		if (currentLineStartWidgetIndex == -1)
		{
			currentLineStartWidgetIndex = currentWidgetIndex;
		}

		return &current;
	}
	else
	{
		currentWidgetIndex = -1;
		return NULL;
	}
}

void Page::FinishCurrentWidget()
{
	if (currentWidgetIndex != -1)
	{
		Widget& current = widgets[currentWidgetIndex];
		cursorX += current.width;
		
		switch (current.type)
		{
		case Widget::Text:
			current.text->text = allocator.AllocString(textBuffer, textBufferSize);
			break;
		}

		textBufferSize = 0;

		currentWidgetIndex = -1;
	}
}

void Page::FinishCurrentLine()
{
	FinishCurrentWidget();

	if (currentLineStartWidgetIndex != -1)
	{
		int lineWidth = 0;
		int lineHeight = 0;

		for (int n = currentLineStartWidgetIndex; n < numWidgets; n++)
		{
			if (widgets[n].height > lineHeight)
			{
				lineHeight = widgets[n].height;
			}
			if (widgets[n].x + widgets[n].width > lineWidth)
			{
				lineWidth = widgets[n].x + widgets[n].width;
			}
		}

		int centerAdjust = (Platform::video->windowWidth - lineWidth) >> 1;

		for (int n = currentLineStartWidgetIndex; n < numWidgets; n++)
		{
			widgets[n].y += lineHeight - widgets[n].height;
			if (widgets[n].style.center)
			{
				widgets[n].x += centerAdjust;
			}
		}

		cursorY += lineHeight;
		pageHeight = cursorY;

		numFinishedWidgets = numWidgets;

		currentLineStartWidgetIndex = -1;
	}

	cursorX = leftMarginPadding;
}

void Page::AppendText(const char* text)
{
	if (currentWidgetIndex == -1 || widgets[currentWidgetIndex].type != Widget::Text)
	{
		CreateTextWidget();
	}

	if (currentWidgetIndex != -1)
	{
		Widget* current = &widgets[currentWidgetIndex];
		Font* font = Platform::video->GetFont(current->style.fontSize);

		int addedWidth = 0;
		int startIndex = 0;

		if (needLeadingWhiteSpace)
		{
			needLeadingWhiteSpace = false;
			if (current->width > 0 && textBufferSize < MAX_TEXT_BUFFER_SIZE)
			{
				textBuffer[textBufferSize++] = ' ';
				current->width += Platform::video->GetGlyphWidth(' ', current->style.fontSize, current->style.fontStyle);
			}
		}

		for (int index = 0; text[index] != '\0'; index++)
		{
			if (text[index] < 32 || text[index] > 128)
				continue;

			int glyphWidth = font->glyphWidth[text[index] - 32];
			if (current->style.fontStyle & FontStyle::Bold)
			{
				glyphWidth++;
			}

			if (current->x + current->width + addedWidth + glyphWidth < Platform::video->windowWidth)
			{
				addedWidth += glyphWidth;

				if (text[index] == ' ')
				{
					// Word boundary, so word fits, append now
					for (int n = startIndex; n <= index; n++)
					{
						if (textBufferSize < MAX_TEXT_BUFFER_SIZE)
						{
							textBuffer[textBufferSize++] = text[n];
						}
					}
					startIndex = index + 1;
					current->width += addedWidth;
					addedWidth = 0;
				}
			}
			else
			{
				if (current->width == 0)
				{
					if (current->x == leftMarginPadding)
					{
						// Case with lots of text without spaces, so just break in middle of the word
						for (int n = startIndex; n < index; n++)
						{
							if (textBufferSize < MAX_TEXT_BUFFER_SIZE)
							{
								textBuffer[textBufferSize++] = text[n];
							}
						}
						current->width += addedWidth;
						startIndex = index + 1;
						addedWidth = 0;

						FinishCurrentLine();

						current = CreateTextWidget();
					}
					else
					{
						// Case where widget is empty and needs to move to the next line

						// Move to new line
						int oldCurrentWidgetIndex = currentWidgetIndex;
						currentWidgetIndex = -1;
						FinishCurrentLine();
						currentWidgetIndex = oldCurrentWidgetIndex;
						currentLineStartWidgetIndex = currentWidgetIndex;
						current->x = cursorX;
						current->y = cursorY;
					}
				}
				else
				{
					// Case where we need to make a new widget on the next line
					FinishCurrentLine();
					current = CreateTextWidget();
				}

				if (!current)
				{
					// Couldn't make a new widget so probably out of memory
					return;
				}

				// Go back a character so it gets processed properly
				index--;
			}
		}

		// Add whatever is left
		if (addedWidth > 0)
		{
			for (int index = startIndex; text[index] != '\0'; index++)
			{
				if (textBufferSize < MAX_TEXT_BUFFER_SIZE)
				{
					textBuffer[textBufferSize++] = text[index];
				}
			}
			current->width += addedWidth;
		}
	}
}

void Page::BreakLine(int padding)
{
	FinishCurrentLine();

	if (padding > pendingVerticalPadding)
	{
		pendingVerticalPadding = padding;
	}
}

void Page::BreakTextLine()
{
	int oldCursorY = cursorY;
	FinishCurrentLine();

	if (oldCursorY == cursorY)
	{
		cursorY += Platform::video->GetLineHeight(GetStyleStackTop().fontSize);
	}
}

void Page::SetTitle(const char* inTitle)
{
	app.renderer.SetTitle(inTitle);
}

Widget* Page::GetWidget(int x, int y)
{
	for (int n = 0; n < numWidgets; n++)
	{
		Widget* widget = &widgets[n];

		if (x >= widget->x && y >= widget->y && x < widget->x + widget->width && y < widget->y + widget->height)
			return widget;
	}
	return NULL;
}

void Page::SetWidgetURL(const char* url)
{
	widgetURL = allocator.AllocString(url);
}

void Page::ClearWidgetURL()
{
	widgetURL = NULL;
}

void Page::FinishSection()
{
	FinishCurrentLine();
}

void Page::SetFormData(WidgetFormData* inFormData)
{
	formData = inFormData;
}
