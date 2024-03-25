#include <string.h>
#include "Png.h"
#include "Image.h"
#include "../Platform.h"

static uint8_t pngSignature[PNG_SIGNATURE_LENGTH] =
{
	137, 80, 78, 71, 13, 10, 26, 10
};

PngDecoder::PngDecoder()
	: internalState(ParseSignature)
{
}

void PngDecoder::Process(uint8_t* data, size_t dataLength)
{
	if (state != ImageDecoder::Decoding)
	{
		return;
	}

	while (dataLength > 0)
	{
		switch (internalState)
		{
		case ParseSignature:
			if (FillStruct(&data, dataLength, signature, PNG_SIGNATURE_LENGTH))
			{
				if(memcmp(signature, pngSignature, PNG_SIGNATURE_LENGTH))
				{
					state = ImageDecoder::Error;
					return;
				}
				internalState = ParseChunkHeader;
			}
			break;
		case ParseChunkHeader:
			if (FillStruct(&data, dataLength, &chunkHeader, sizeof(ChunkHeader)))
			{
				if (!memcmp(chunkHeader.type, "IEND", 4))
				{
					state = ImageDecoder::Success;
					return;
				}
				if (!memcmp(chunkHeader.type, "IHDR", 4))
				{
					internalState = ParseImageHeader;
				}
				else
				{
					internalState = SkipChunk;
				}
			}
			break;
		case SkipChunk:
			if (SkipBytes(&data, dataLength, chunkHeader.length + 4))		// +4 as we are just going to skip CRC check
			{
				internalState = ParseChunkHeader;
			}
			break;
		case ParseImageHeader:
			if (FillStruct(&data, dataLength, &imageHeader, sizeof(ImageHeader)))
			{
				if (outputImage->width == 0 && outputImage->height == 0)
				{
					int width = imageHeader.width;
					int height = imageHeader.height;
					Platform::video->ScaleImageDimensions(width, height);

					outputImage->width = width;
					outputImage->height = height;

					if (onlyDownloadDimensions)
					{
						state = ImageDecoder::Success;
						return;
					}
				}

				state = ImageDecoder::Success;
				return;
			}
			break;
		}
	}
}

