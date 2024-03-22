#include <memory.h>
#include <stdio.h>
#include <string.h>
#include "DataPack.h"
#include "Platform.h"
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

bool DataPack::LoadPreset(DataPack::Preset preset)
{
	return Load(datapackFilenames[preset]);
}


bool DataPack::Load(const char* path)
{
	FILE* fs = fopen(path, "rb");

	if (!fs)
	{
		Platform::FatalError("Could not load %s", path);
		return false;
	}

	DataPackHeader header;

	fread(&header.numEntries, sizeof(uint16_t), 1, fs);
	header.entries = new DataPackEntry[header.numEntries];

	if (!header.entries)
	{
		Platform::FatalError("Could not allocate memory for data pack header from %s", path);
		return false;
	}

	fread(header.entries, sizeof(DataPackEntry), header.numEntries, fs);

	pointerCursor = (MouseCursorData*)LoadAsset(fs, header, "CMOUSE");
	linkCursor = (MouseCursorData*)LoadAsset(fs, header, "CLINK");
	textSelectCursor = (MouseCursorData*)LoadAsset(fs, header, "CTEXT");

	imageIcon = LoadImageAsset(fs, header, "IIMG");
	brokenImageIcon = LoadImageAsset(fs, header, "IBROKEN");
	bulletIcon = LoadImageAsset(fs, header, "IBULLET");

	fonts[0] = (Font*)LoadAsset(fs, header, "FHELV1");
	fonts[1] = (Font*)LoadAsset(fs, header, "FHELV2");
	fonts[2] = (Font*)LoadAsset(fs, header, "FHELV3");
	monoFonts[0] = (Font*)LoadAsset(fs, header, "FCOUR1");
	monoFonts[1] = (Font*)LoadAsset(fs, header, "FCOUR2");
	monoFonts[2] = (Font*)LoadAsset(fs, header, "FCOUR3");

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
		return monoFonts[FontSizeToIndex(fontSize)];
	}
	else
	{
		return fonts[FontSizeToIndex(fontSize)];
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
		if (!image)
		{
			Platform::FatalError("Could not allocate memory for data pack image %s", entryName);
		}
		memcpy(image, asset, sizeof(ImageMetadata));
		uint8_t* data = ((uint8_t*) asset) + sizeof(ImageMetadata);
		MemBlockHandle* lines = new MemBlockHandle[image->height];
		if (!lines)
		{
			Platform::FatalError("Could not allocate memory for data pack image %s", entryName);
		}
		for (int y = 0; y < image->height; y++)
		{
			lines[y] = MemBlockHandle(data + y * image->pitch);
		}
		image->lines = MemBlockHandle(lines);

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
	if (!buffer)
	{
		Platform::FatalError("Could not allocate memory for data pack asset %s", entryName);
	}

	fread(buffer, length, 1, fs);

	return buffer;
}

