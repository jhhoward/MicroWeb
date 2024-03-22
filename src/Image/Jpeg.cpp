#include "Jpeg.h"
#include "Image.h"
#include "../Platform.h"

// Start of Frame markers, non-differential, Huffman coding
const uint8_t SOF0 = 0xC0; // Baseline DCT
const uint8_t SOF1 = 0xC1; // Extended sequential DCT
const uint8_t SOF2 = 0xC2; // Progressive DCT
const uint8_t SOF3 = 0xC3; // Lossless (sequential)

// Start of Frame markers, differential, Huffman coding
const uint8_t SOF5 = 0xC5; // Differential sequential DCT
const uint8_t SOF6 = 0xC6; // Differential progressive DCT
const uint8_t SOF7 = 0xC7; // Differential lossless (sequential)

// Start of Frame markers, non-differential, arithmetic coding
const uint8_t SOF9 = 0xC9; // Extended sequential DCT
const uint8_t SOF10 = 0xCA; // Progressive DCT
const uint8_t SOF11 = 0xCB; // Lossless (sequential)

// Start of Frame markers, differential, arithmetic coding
const uint8_t SOF13 = 0xCD; // Differential sequential DCT
const uint8_t SOF14 = 0xCE; // Differential progressive DCT
const uint8_t SOF15 = 0xCF; // Differential lossless (sequential)

const uint8_t SOI = 0xD8; // Start of Image
const uint8_t EOI = 0xD9; // End of Image

JpegDecoder::JpegDecoder()
	: internalState(ParseStartMarker)
{
}

void JpegDecoder::Process(uint8_t* data, size_t dataLength)
{
	if (state != ImageDecoder::Decoding)
	{
		return;
	}

	while (dataLength > 0)
	{
		switch (internalState)
		{
		case ParseStartMarker:
			if (FillStruct(&data, dataLength, &marker, sizeof(marker)))
			{
				if (marker.highByte != 0xff || marker.lowByte != SOI)
				{
					state = ImageDecoder::Error;
					return;
				}
				internalState = ParseMarker;
			}
			break;
		case ParseMarker:
			if (FillStruct(&data, dataLength, &marker, sizeof(marker)))
			{
				if (marker.highByte != 0xff)
				{
					state = ImageDecoder::Error;
					return;
				}

				switch (marker.lowByte)
				{
				case EOI:
					state = ImageDecoder::Success;
					return;
				case SOF0:
				case SOF2:
					internalState = ParseStartOfFrame;
					break;
				default:
					internalState = ParseSegmentLength;
					break;
				}
			}
			break;
		case ParseSegmentLength:
			if (FillStruct(&data, dataLength, &segmentLength, sizeof(segmentLength)))
			{
				segmentLength = segmentLength - 2;	// Reduce by 2 uint8_ts for the length value
				internalState = SkipSegment;
			}
			break;
		case SkipSegment:
			if(SkipBytes(&data, dataLength, segmentLength))
			{
				internalState = ParseMarker;
			}
			break;
		case ParseStartOfFrame:
			if (FillStruct(&data, dataLength, &frameHeader, sizeof(FrameHeader)))
			{
				if (outputImage->width == 0 && outputImage->height == 0)
				{
					int width = frameHeader.width;
					int height = frameHeader.height;
					Platform::video->ScaleImageDimensions(width, height);

					outputImage->width = width;
					outputImage->height = height;

					if (onlyDownloadDimensions)
					{
						state = ImageDecoder::Success;
						return;
					}
				}

				state = ImageDecoder::Error;
				return;
			}
			break;
		}
	}
}

