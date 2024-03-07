#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "DataPack.h"
#include <malloc.h>

DataPack Assets;

#pragma warning(disable:4996)

const char* DataPack::datapackFilenames[] =
{
	"CGA.DAT",
	"EGA.DAT",
	"DEFAULT.DAT",
	"LOWRES.DAT"
};

bool DataPack::LoadPreset(DataPack::Preset preset, bool forceBold)
{
	return Load(datapackFilenames[preset], forceBold);
}


bool DataPack::Load(const char* path, bool forceBold)
{
	FILE* fs = fopen(path, "rb");

	if (!fs)
	{
		return false;
	}

	DataPackHeader header;

	fread(&header.numEntries, sizeof(uint16_t), 1, fs);
	header.entries = new DataPackEntry[header.numEntries];
	fread(header.entries, sizeof(DataPackEntry), header.numEntries, fs);

	pointerCursor = (MouseCursorData*)LoadAsset(fs, header, "CMOUSE");
	linkCursor = (MouseCursorData*)LoadAsset(fs, header, "CLINK");
	textSelectCursor = (MouseCursorData*)LoadAsset(fs, header, "CTEXT");

	imageIcon = LoadImageAsset(fs, header, "IIMG");

	boldFonts[0] = (Font*)LoadAsset(fs, header, "FHELV1B");
	boldFonts[1] = (Font*)LoadAsset(fs, header, "FHELV2B");
	boldFonts[2] = (Font*)LoadAsset(fs, header, "FHELV3B");
	boldMonoFonts[0] = (Font*)LoadAsset(fs, header, "FCOUR1B");
	boldMonoFonts[1] = (Font*)LoadAsset(fs, header, "FCOUR2B");
	boldMonoFonts[2] = (Font*)LoadAsset(fs, header, "FCOUR3B");

	if (forceBold)
	{
		fonts[0] = boldFonts[0];
		fonts[1] = boldFonts[1];
		fonts[2] = boldFonts[2];
		monoFonts[0] = boldMonoFonts[0];
		monoFonts[1] = boldMonoFonts[1];
		monoFonts[2] = boldMonoFonts[2];
	}
	else
	{
		fonts[0] = (Font*)LoadAsset(fs, header, "FHELV1");
		fonts[1] = (Font*)LoadAsset(fs, header, "FHELV2");
		fonts[2] = (Font*)LoadAsset(fs, header, "FHELV3");
		monoFonts[0] = (Font*)LoadAsset(fs, header, "FCOUR1");
		monoFonts[1] = (Font*)LoadAsset(fs, header, "FCOUR2");
		monoFonts[2] = (Font*)LoadAsset(fs, header, "FCOUR3");
	}


	delete[] header.entries;
	fclose(fs);

	return true;
}

int DataPack::FontSizeToIndex(int fontSize)
{
	switch (fontSize)
	{
	case 0:
		return 0;
	case 2:
	case 3:
	case 4:
		return 2;
	default:
		return 1;
	}
}

Font* DataPack::GetFont(int fontSize, FontStyle::Type fontStyle)
{
	if (fontStyle & FontStyle::Monospace)
	{
		if (fontStyle & FontStyle::Bold)
		{
			return boldMonoFonts[FontSizeToIndex(fontSize)];
		}
		else
		{
			return monoFonts[FontSizeToIndex(fontSize)];
		}
	}
	else
	{
		if (fontStyle & FontStyle::Bold)
		{
			return boldFonts[FontSizeToIndex(fontSize)];
		}
		else
		{
			return fonts[FontSizeToIndex(fontSize)];
		}
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

Image* DataPack::LoadImageAsset(FILE* fs, DataPackHeader& header, const char* entryName)
{
	void* asset = LoadAsset(fs, header, entryName);
	if (asset)
	{
		Image* image = new Image();
		memcpy(image, asset, sizeof(ImageMetadata));
		//image->data = ((uint8_t*) asset) + sizeof(ImageMetadata);
		return image;
	}
	return nullptr;
}

void* DataPack::LoadAsset(FILE* fs, DataPackHeader& header, const char* entryName, void* buffer)
{
	DataPackEntry* entry = NULL;

	for (int n = 0; n < header.numEntries; n++)
	{
		if (!stricmp(header.entries[n].name, entryName))
		{
			entry = &header.entries[n];
			break;
		}
	}

	if (!entry)
	{
		return NULL;
	}

	fseek(fs, entry->offset, SEEK_SET);
	int32_t length = entry[1].offset - entry[0].offset;

	if (!buffer)
	{
		buffer = malloc(length);
	}

	fread(buffer, length, 1, fs);

	return buffer;
}

