#pragma once

#include <stdint.h>

struct FontStyle
{
	enum Type
	{
		Regular = 0,
		Bold = 1,
		Italic = 2,
		Underline = 4,
	};
};

struct Font
{
	uint8_t glyphWidth[96];
	uint8_t glyphWidthBytes;
	uint8_t glyphHeight;
	uint8_t glyphDataStride;
	uint8_t* glyphData;

	int CalculateWidth(const char* text, FontStyle::Type style = FontStyle::Regular);
};

