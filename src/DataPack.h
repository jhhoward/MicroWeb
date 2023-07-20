#ifndef _DATAPACK_H_
#define _DATAPACK_H_

#include <stdint.h>
#include "Cursor.h"
#include "Image.h"
#include "Font.h"

/*

Asset data pack format

Header: (all offsets relative to end of the header)
uint16 smallFontOffset
uint16 mediumFontOffset
uint16 largeFontOffset
uint16 smallMonoFontOffset
uint16 mediumMonoFontOffset
uint16 largeMonoFontOffset
uint16 pointerCursorOffset
uint16 linkCursorOffset
uint16 textSelectCursorOffset

Mouse cursor:
uint16 hotspotX
uint16 hotspotY
uint16 cursorData[32]

Font data:
uint8 glyphWidth[256 - 32]
uint8 glyphWidthBytes
uint8 glyphHeight
uint8 glyphDataStride
uint8 glyphData[variable]

*/

#define NUM_FONT_SIZES 3

struct DataPackHeader
{
	uint16_t fontOffsets[NUM_FONT_SIZES];
	uint16_t monoFontOffsets[NUM_FONT_SIZES];
	uint16_t pointerCursorOffset;
	uint16_t linkCursorOffset;
	uint16_t textSelectCursorOffset;
	uint16_t imageIconOffset;
};

struct DataPack
{
	MouseCursorData* pointerCursor;
	MouseCursorData* linkCursor;
	MouseCursorData* textSelectCursor;
	Image* imageIcon;

	Font fonts[NUM_FONT_SIZES];
	Font monoFonts[NUM_FONT_SIZES];

	bool Load(const char* path);
	Font* GetFont(int fontSize, FontStyle::Type fontStyle);
	MouseCursorData* GetMouseCursorData(MouseCursor::Type type);
};

extern DataPack Assets;

#endif
