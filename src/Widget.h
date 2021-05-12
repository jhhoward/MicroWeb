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

struct Widget
{
	uint8_t type;
	uint16_t x, y;
	uint16_t width, height;
	WidgetStyle style;

	union
	{
		char* text;
	};
};
