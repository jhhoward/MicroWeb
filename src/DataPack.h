#ifndef _DATAPACK_H_
#define _DATAPACK_H_

#include <stdio.h>
#include <stdint.h>
#include "Cursor.h"
#include "Image/Image.h"
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

struct DataPackEntry
{
	char name[8];
	uint32_t offset;
};

struct DataPackHeader
{
	uint16_t numEntries;
	DataPackEntry* entries;
};

struct DataPack
{
	enum Preset
	{
		CGA,
		EGA,
		Default,
		Lowres
	};

	MouseCursorData* pointerCursor;
	MouseCursorData* linkCursor;
	MouseCursorData* textSelectCursor;
	Image* imageIcon;
	Image* brokenImageIcon;

	Image* checkbox;
	Image* checkboxTicked;
	Image* radio;
	Image* radioSelected;

	Font* fonts[NUM_FONT_SIZES];
	Font* monoFonts[NUM_FONT_SIZES];

	bool LoadPreset(Preset preset);
	bool Load(const char* path);
	Font* GetFont(int fontSize, FontStyle::Type fontStyle);
	MouseCursorData* GetMouseCursorData(MouseCursor::Type type);

private:
	void* LoadAsset(FILE* fs, DataPackHeader& header, const char* entryName, void* buffer = NULL);
	Image* LoadImageAsset(FILE* fs, DataPackHeader& header, const char* entryName);
	int FontSizeToIndex(int fontSize);
	static const char* datapackFilenames[];
};

extern DataPack Assets;

#endif
