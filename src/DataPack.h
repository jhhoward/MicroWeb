#pragma once

#include "Image.h"
#include "Font.h"
#include "Cursor.h"

struct DataPack
{
	MouseCursorData cursor;
	MouseCursorData hand;
	MouseCursorData select;
	Image imageIcon;
	Font small;
	Font medium;
	Font large;

	void Fixup()
	{
		imageIcon.data += (uint8_t*)(this);
		small.glyphData += (uint8_t*)(this);
		medium.glyphData += (uint8_t*)(this);
		large.glyphData += (uint8_t*)(this);
	}
};
