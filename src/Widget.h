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

	Type type : 8;
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
	};
};

