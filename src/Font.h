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
	uint8_t glyphWidth[96];
	uint8_t glyphWidthBytes;
	uint8_t glyphHeight;
	uint8_t glyphDataStride;
	uint8_t* glyphData;

	int CalculateWidth(const char* text, FontStyle::Type style = FontStyle::Regular);
	int GetGlyphWidth(char c) { return glyphWidth[c - 32]; }
};

