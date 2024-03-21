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

#include "Font.h"

int Font::CalculateWidth(const char* text, FontStyle::Type style)
{
	int result = 0;

	uint8_t bold = (style & FontStyle::Bold) ? 1 : 0;

	while (*text)
	{
		char c = *text++;

		int index = (unsigned char)c - FIRST_FONT_GLYPH;
		if (index < 0)
		{
			continue;
		}

		uint8_t width = glyphs[index].width;
		if (width)
		{
			result += width + bold;
		}
	}

	return result;
}

int Font::GetGlyphWidth(char c, FontStyle::Type style)
{
	int index = (unsigned char)(c)-FIRST_FONT_GLYPH;
	if (index < 0)
	{
		return 0;
	}
	int result = glyphs[index].width;

	if (result)
	{
		if (style & FontStyle::Bold)
			result++;
	}
	return result;
}
