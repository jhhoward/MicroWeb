#ifndef _JPEG_H_
#define _JPEG_H_

#include "Decoder.h"

class JpegDecoder : public ImageDecoder
{
public:
	JpegDecoder();
	virtual void Process(uint8_t* data, size_t dataLength) override;

private:
	enum InternalState
	{
		ParseStartMarker,
		ParseMarker,
		ParseSegmentLength,
		SkipSegment,
		ParseStartOfFrame
	};

#pragma pack(push, 1)
	struct FrameHeader
	{
		uint16_be length;
		uint8_t bpp;
		uint16_be height;
		uint16_be width;
		uint8_t numComponents;
	};
#pragma pack(pop)

	InternalState internalState;

	uint16_be marker;
	uint16_be segmentLength;

	FrameHeader frameHeader;
};

#endif
