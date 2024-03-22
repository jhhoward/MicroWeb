#ifndef _PNG_H_
#define _PNG_H_

#include "Decoder.h"

#define PNG_SIGNATURE_LENGTH 8

class PngDecoder : public ImageDecoder
{
public:
	PngDecoder();
	virtual void Process(uint8_t* data, size_t dataLength) override;

private:
	enum InternalState
	{
		ParseSignature,
		ParseChunkHeader,
		SkipChunk,
		ParseImageHeader,
	};

#pragma pack(push, 1)
	struct ChunkHeader
	{
		uint32_be length;
		char type[4];
	};

	struct ImageHeader
	{
		uint32_be width;
		uint32_be height;
		uint8_t bitDepth;
		uint8_t colourType;
		uint8_t compressionMethod;
		uint8_t filterMethod;
		uint8_t interlaceMode;
	};
#pragma pack(pop)

	InternalState internalState;

	uint8_t signature[PNG_SIGNATURE_LENGTH];
	ChunkHeader chunkHeader;
	ImageHeader imageHeader;
};

#endif
