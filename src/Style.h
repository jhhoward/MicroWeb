#pragma once
#include <stdint.h>

enum StyleMaskTypes
{
	Style_Bold = 1,
	Style_Italic = 2,
	Style_Underline = 4,
	Style_H1 = 8,
	Style_H2 = 16,
	Style_H3 = 32,
	Style_H4 = 64,
	Style_Center = 128
};

typedef uint16_t StyleMask;
