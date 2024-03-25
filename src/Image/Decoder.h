#ifndef _DECODER_H_
#define _DECODER_H_

#include <stdint.h>
#include <stddef.h>
struct Image;
class LinearAllocator;

#pragma pack(push, 1)
struct uint16_be
{
	uint16_be() {}
	uint16_be(uint16_t x) : highByte((uint8_t)(x >> 8)), lowByte((uint8_t)x) {}

	uint8_t highByte;
	uint8_t lowByte;

	operator uint16_t ()
	{
		return (highByte << 8) | lowByte;
	}
};

struct uint32_be
{
	uint32_be() {}
	uint32_be(uint32_t x)
	{
		bytes[0] = (uint8_t)(x & 0xff);
		bytes[1] = (uint8_t)((x >> 8) & 0xff);
		bytes[2] = (uint8_t)((x >> 16) & 0xff);
		bytes[3] = (uint8_t)((x >> 24) & 0xff);
	}

	uint8_t bytes[4];

	operator uint32_t ()
	{
		return
			((uint32_t)bytes[0] << 24) |
			((uint32_t)bytes[1] << 16) |
			((uint32_t)bytes[2] << 8) |
			((uint32_t)bytes[3]);
	}
};
#pragma pack(pop)

class ImageDecoder
{
public:
	enum State
	{
		Stopped,
		Decoding,
		Success,
		Error
	};

	enum DecoderType
	{
		Gif,
		Png,
		Jpeg
	};

	ImageDecoder() : outputImage(NULL), state(Stopped), structFillPosition(0) {}
	
	void Begin(Image* image, bool dimensionsOnly);
	virtual void Process(uint8_t* data, size_t dataLength) = 0;
	State GetState() { return state; }
	
	static void Allocate();
	static ImageDecoder* Get();
	static ImageDecoder* Create(DecoderType type);
	static ImageDecoder* CreateFromExtension(const char* path);
	static ImageDecoder* CreateFromMIME(const char* type);

protected:
	bool FillStruct(uint8_t** data, size_t& dataLength, void* dest, size_t size);
	uint8_t NextByte(uint8_t** data, size_t& dataLength)
	{
		uint8_t result = **data;
		(*data)++;
		dataLength--;
		return result;
	}
	bool SkipBytes(uint8_t** data, size_t& dataLength, size_t size);
	size_t structFillPosition;

	Image* outputImage;
	ImageDecoder::State state;
	bool onlyDownloadDimensions;

	static const uint8_t greyDitherMatrix[256];
	static const int8_t colourDitherMatrix[16];

};



#endif
