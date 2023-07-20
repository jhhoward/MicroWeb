#include <memory.h>
#include <stdio.h>
#include "DataPack.h"

DataPack Assets;

#pragma warning(disable:4996)

bool DataPack::Load(const char* path)
{
	FILE* fs = fopen(path, "rb");

	if (!fs)
	{
		return false;
	}

	fseek(fs, 0, SEEK_END);
	long dataPackSize = ftell(fs);
	fseek(fs, 0, SEEK_SET);

	if (dataPackSize <= sizeof(DataPackHeader))
	{
		return false;
	}

	DataPackHeader header;

	fread(&header, sizeof(DataPackHeader), 1, fs);

	long rawDataSize = dataPackSize - sizeof(DataPackHeader);
	uint8_t* rawData = new uint8_t[rawDataSize];
	if (!rawData)
	{
		return false;
	}

	fread(rawData, 1, rawDataSize, fs);

	fclose(fs);

	pointerCursor = (MouseCursorData*)(&rawData[header.pointerCursorOffset]);
	linkCursor = (MouseCursorData*)(&rawData[header.linkCursorOffset]);
	textSelectCursor = (MouseCursorData*)(&rawData[header.textSelectCursorOffset]);

	imageIcon = (Image*)(&rawData[header.imageIconOffset]);

	for (int n = 0; n < NUM_FONT_SIZES; n++)
	{
		memcpy(&fonts[n], &rawData[header.fontOffsets[n]], sizeof(FontMetaData));
		fonts[n].glyphData = &rawData[header.fontOffsets[n] + sizeof(FontMetaData)];

		memcpy(&monoFonts[n], &rawData[header.monoFontOffsets[n]], sizeof(FontMetaData));
		monoFonts[n].glyphData = &rawData[header.monoFontOffsets[n] + sizeof(FontMetaData)];
	}

	return true;
}

Font* DataPack::GetFont(int fontSize, FontStyle::Type fontStyle)
{
	if (fontStyle & FontStyle::Monospace)
	{
		switch (fontSize)
		{
		case 0:
			return &monoFonts[0];
		case 2:
		case 3:
		case 4:
			return &monoFonts[2];
		default:
			return &monoFonts[1];
		}
	}

	switch (fontSize)
	{
	case 0:
		return &fonts[0];
	case 2:
	case 3:
	case 4:
		return &fonts[2];
	default:
		return &fonts[1];
	}

}

MouseCursorData* DataPack::GetMouseCursorData(MouseCursor::Type type)
{
	switch (type)
	{
	case MouseCursor::Hand:
		return linkCursor;
	case MouseCursor::Pointer:
		return pointerCursor;
	case MouseCursor::TextSelect:
		return textSelectCursor;
	}
	return NULL;
}
