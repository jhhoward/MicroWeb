#ifndef _GIF_H_
#define _GIF_H_

#include <stdint.h>
#include "Decoder.h"
#include "../LinAlloc.h"

#define GIF_MAX_LZW_CODE_LENGTH 12
#define GIF_MAX_DICTIONARY_ENTRIES (1 << (GIF_MAX_LZW_CODE_LENGTH + 1))

#define GIF_INTERLACE_BIT 0x40

class GifDecoder : public ImageDecoder
{
public:
	GifDecoder(LinearAllocator& inAllocator);
	
	virtual void Begin(Image* image);
	virtual void Process(uint8_t* data, size_t dataLength);
	virtual ImageDecoder::State GetState() { return state; }
	
private:
	bool FillStruct(uint8_t** data, size_t& dataLength, void* dest, size_t size);
	uint8_t NextByte(uint8_t** data, size_t& dataLength)
	{
		uint8_t result = **data;
		(*data)++;
		dataLength--;
		return result;
	}
	bool SkipBytes(uint8_t** data, size_t& dataLength, size_t size);

	void ClearDictionary();
	void OutputPixel(uint8_t pixelValue);
	int CalculateLineIndex(int y);


	enum InternalState
	{
		ParseHeader,
		ParsePalette,
		ParseImageDescriptor,
		ParseLocalColourTable,
		ParseLZWCodeSize,
		ParseDataBlock,
		ParseImageSubBlockSize,
		ParseImageSubBlock,
		ParseExtension,
		ParseExtensionContents,
		ParseExtensionSubBlockSize,
		ParseExtensionSubBlock,
	};

	#pragma pack(push, 1)
	struct Header
	{
		char versionTag[6];		// Should be GIF89a
		uint16_t width;
		uint16_t height;
		uint8_t fields;
		uint8_t backgroundColour;
		uint8_t aspectRatio;
	};

	struct ImageDescriptor
	{
		uint16_t x, y;
		uint16_t width, height;
		uint8_t fields;
	};

	struct ExtensionHeader
	{
		uint8_t code;
		uint8_t size;
	};
	
	struct DictionaryEntry
	{
		uint8_t byte;
		int32_t prev;
		int len;
	};
	#pragma pack(pop)

	LinearAllocator& allocator;
	ImageDecoder::State state;
	InternalState internalState;
	
	uint8_t palette[256*3];			// RGB values
	uint8_t paletteLUT[256];		// GIF palette colour to video mode palette colour
	int paletteSize;
	uint8_t backgroundColour;
	uint8_t lzwCodeSize;

	size_t structFillPosition;
	
	DictionaryEntry dictionary[GIF_MAX_DICTIONARY_ENTRIES];

//	union
	//{
		struct
		{
			// ParseHeader temporary vars
			Header header;
		};
		struct
		{
			// ParsePalette temporary vars
			unsigned int paletteIndex;
			uint8_t rgb[3];
			int localColourTableLength;
		};
		struct
		{
			// ParseImageDescriptor temporary vars
			ImageDescriptor imageDescriptor;
			uint8_t imageSubBlockSize;

			int codeLength;
			int resetCodeLength;
			int clearCode;
			int stopCode;
			int code;
			int prev;
			int dictionaryIndex;
			int codeBit;
			
			int drawX, drawY;
			int writePosition;
		};
		struct
		{
			// ParseExtension  temporary vars
			ExtensionHeader extensionHeader;
			uint8_t extensionSubBlockSize;
		};		
	//};
};

#endif
