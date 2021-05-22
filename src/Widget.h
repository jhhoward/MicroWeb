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

#pragma once
#include <stdint.h>
#include "Font.h"

struct WidgetStyle
{
	WidgetStyle() {}
	WidgetStyle(FontStyle::Type inFontStyle, uint8_t inFontSize = 1, bool inCenter = false) :
		fontStyle(inFontStyle), fontSize(inFontSize), center(inCenter) {}

	FontStyle::Type fontStyle : 3;
	uint8_t fontSize : 2;
	bool center : 1;
};

struct TextWidgetData
{
	char* text;
	char* linkURL;
};

struct WidgetFormData
{
	enum MethodType
	{
		Get,
		Post
	};
	char* action;
	MethodType method;
};

struct ButtonWidgetData
{
	char* text;
	WidgetFormData* form;
};

struct TextFieldWidgetData
{
	char* buffer;
	char* name;
	int bufferLength;
	WidgetFormData* form;
};

struct ScrollBarData
{
	int position;
	int size;
};

struct Widget
{
	enum Type
	{
		Text,
		HorizontalRule,
		Button,
		TextField,
		ScrollBar
	};

	Widget() : type(Text), isInterfaceWidget(false), x(0), y(0), width(0), height(0) {}

	Type type : 7;
	bool isInterfaceWidget : 1;
	uint16_t x, y;
	uint16_t width, height;
	WidgetStyle style;

	char* GetLinkURL()
	{
		if (type == Text)
		{
			return text->linkURL;
		}
		return NULL;
	}

	union
	{
		TextWidgetData* text;
		ButtonWidgetData* button;
		TextFieldWidgetData* textField;
		ScrollBarData* scrollBar;
	};
};

