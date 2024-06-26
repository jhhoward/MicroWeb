#ifndef _GIF_H_
#define _GIF_H_

#include <stdint.h>
#include "Decoder.h"

#define GIF_MAX_LZW_CODE_LENGTH 12
#define GIF_MAX_DICTIONARY_ENTRIES (1 << (GIF_MAX_LZW_CODE_LENGTH + 1))

#define GIF_INTERLACE_BIT 0x40
#define GIF_LINE_BUFFER_MAX_SIZE 640

class GifDecoder : public ImageDecoder
{
public:
	GifDecoder();
	
	virtual void Process(uint8_t* data, size_t dataLength);
	
private:

	void ClearDictionary();
	
	int CalculateLineIndex(int y);
	void ProcessLineBuffer();
	void EmitLine(int y);


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
		ParseGraphicControlExtension
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
		int16_t prev;
		uint16_t len;
	};

	struct GraphicControlExtension
	{
		uint8_t packedFields;
		uint16_t delayTime;
		uint8_t transparentColourIndex;
	};
	#pragma pack(pop)

	InternalState internalState;
	
	uint8_t palette[256*3];			// RGB values
	uint8_t paletteLUT[256];		// GIF palette colour to video mode palette colour
	int paletteSize;
	uint8_t backgroundColour;
	int transparentColourIndex;
	uint8_t lzwCodeSize;
	
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
			int outputLine;

			uint8_t lineBuffer[GIF_LINE_BUFFER_MAX_SIZE];
			int lineBufferSize;
			int linesProcessed;
			int lineBufferDivider;
			int lineBufferSkipCount;
			int lineBufferFlushCount;
		};
		struct
		{
			// ParseExtension  temporary vars
			ExtensionHeader extensionHeader;
			uint8_t extensionSubBlockSize;
			GraphicControlExtension graphicControlExtension;
		};		
	//};
};

#endif
