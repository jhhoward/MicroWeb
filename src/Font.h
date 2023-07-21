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

#ifndef _FONT_H_
#define _FONT_H_

#include <stdint.h>

#define FIRST_FONT_GLYPH 32
#define LAST_FONT_GLYPH 255

struct FontStyle
{
	enum Type
	{
		Regular = 0,
		Bold = 1,
		Italic = 2,
		Underline = 4,
		Monospace = 8
	};
};

struct Font 
{
	uint8_t glyphWidth[LAST_FONT_GLYPH + 1 - FIRST_FONT_GLYPH];
	uint8_t glyphWidthBytes;
	uint8_t glyphHeight;
	uint8_t glyphDataStride;
	uint8_t glyphData[1];

	int CalculateWidth(const char* text, FontStyle::Type style = FontStyle::Regular);
	int GetGlyphWidth(char c, FontStyle::Type style = FontStyle::Regular);
};

#endif
