#include <new>
#include <memory.h>
#include <string.h>
#include "Gif.h"
#include "Jpeg.h"
#include "Png.h"
#include "Decoder.h"
#include "../Platform.h"
#include "../VidModes.h"
#include "Image.h"
#include "../Draw/Surface.h"
#pragma warning(disable:4996)

#include <stdio.h>
#include <conio.h>

typedef union 
{
	char gif[sizeof(GifDecoder)];
	char png[sizeof(PngDecoder)];
	char jpeg[sizeof(JpegDecoder)];
	char buffer[1];
} ImageDecoderUnion;

static ImageDecoderUnion* imageDecoderUnion = nullptr;

#define COLDITH(x) ((x * 4) - 32)

const int8_t ImageDecoder::colourDitherMatrix[16] = 
{ 
//    0,  8,  2, 10,
//    12,  4, 14,  6,
//    3, 11,  1,  9,
//    15,  7, 13,  5 
    COLDITH(0),     COLDITH(8),      COLDITH(2),      COLDITH(10),
    COLDITH(12),    COLDITH(4),      COLDITH(14),     COLDITH(6),
    COLDITH(3),     COLDITH(11),     COLDITH(1),      COLDITH(9),
    COLDITH(15),    COLDITH(7),      COLDITH(13),     COLDITH(5)
};

const uint8_t ImageDecoder::greyDitherMatrix[256] = 
{ 
    0, 128,  32, 160,   8, 136,  40, 168,   2, 130,  34, 162,  10, 138,  42, 170,
    192,  64, 224,  96, 200,  72, 232, 104, 194,  66, 226,  98, 202,  74, 234, 106,
    48, 176,  16, 144,  56, 184,  24, 152,  50, 178,  18, 146,  58, 186,  26, 154,
    240, 112, 208,  80, 248, 120, 216,  88, 242, 114, 210,  82, 250, 122, 218,  90,
    12, 140,  44, 172,   4, 132,  36, 164,  14, 142,  46, 174,   6, 134,  38, 166,
    204,  76, 236, 108, 196,  68, 228, 100, 206,  78, 238, 110, 198,  70, 230, 102,
    60, 188,  28, 156,  52, 180,  20, 148,  62, 190,  30, 158,  54, 182,  22, 150,
    252, 124, 220,  92, 244, 116, 212,  84, 254, 126, 222,  94, 246, 118, 214,  86,
    3, 131,  35, 163,  11, 139,  43, 171,   1, 129,  33, 161,   9, 137,  41, 169,
    195,  67, 227,  99, 203,  75, 235, 107, 193,  65, 225,  97, 201,  73, 233, 105,
    51, 179,  19, 147,  59, 187,  27, 155,  49, 177,  17, 145,  57, 185,  25, 153,
    243, 115, 211,  83, 251, 123, 219,  91, 241, 113, 209,  81, 249, 121, 217,  89,
    15, 143,  47, 175,   7, 135,  39, 167,  13, 141,  45, 173,   5, 133,  37, 165,
    207,  79, 239, 111, 199,  71, 231, 103, 205,  77, 237, 109, 197,  69, 229, 101,
    63, 191,  31, 159,  55, 183,  23, 151,  61, 189,  29, 157,  53, 181,  21, 149,
    254, 127, 223,  95, 247, 119, 215,  87, 253, 125, 221,  93, 245, 117, 213,  85 
};

void ImageDecoder::Allocate()
{
    if (!imageDecoderUnion)
    {
        imageDecoderUnion = new ImageDecoderUnion;
        
        
        //printf("Decoder size: %u bytes", (long) sizeof(ImageDecoderUnion));
        //getch();
    }
    if (!imageDecoderUnion)
    {
        Platform::FatalError("Could not allocate memory for image decoder");
    }
}

ImageDecoder* ImageDecoder::Get()
{
	return (ImageDecoder*)(imageDecoderUnion->buffer);
}

ImageDecoder* ImageDecoder::Create(DecoderType type)
{
    switch (type)
    {
    case ImageDecoder::Jpeg:
        return new (imageDecoderUnion->buffer) JpegDecoder();
    case ImageDecoder::Gif:
        return new (imageDecoderUnion->buffer) GifDecoder();
    case ImageDecoder::Png:
        return new (imageDecoderUnion->buffer) PngDecoder();
    default:
        return nullptr;
    }
}

ImageDecoder* ImageDecoder::CreateFromMIME(const char* type)
{
    if (!stricmp(type, "image/gif"))
    {
        return Create(ImageDecoder::Gif);
    }
    if (!stricmp(type, "image/png"))
    {
        return Create(ImageDecoder::Png);
    }
    if (!stricmp(type, "image/jpeg"))
    {
        return Create(ImageDecoder::Jpeg);
    }
    return nullptr;
}

ImageDecoder* ImageDecoder::CreateFromExtension(const char* path)
{
    const char* extension = path + strlen(path);
    while (extension > path)
    {
        extension--;
        if (*extension == '.')
        {
            extension++;
            if (!stricmp(extension, "gif"))
            {
                return Create(ImageDecoder::Gif);
            }
            if (!stricmp(extension, "png"))
            {
                return Create(ImageDecoder::Png);
            }
            if (!stricmp(extension, "jpeg") || !stricmp(extension, "jpg"))
            {
                return Create(ImageDecoder::Jpeg);
            }

            return nullptr;
        }
    }

    return nullptr;
}

bool ImageDecoder::FillStruct(uint8_t** data, size_t& dataLength, void* dest, size_t size)
{
    size_t bytesLeft = size - structFillPosition;
    if (bytesLeft <= dataLength)
    {
        memcpy((uint8_t*)dest + structFillPosition, *data, bytesLeft);
        *data += bytesLeft;
        dataLength -= bytesLeft;
        structFillPosition = 0;
        return true;
    }
    else
    {
        memcpy((uint8_t*)dest + structFillPosition, *data, dataLength);
        *data += dataLength;
        structFillPosition += dataLength;
        dataLength = 0;
        return false;
    }
}

bool ImageDecoder::SkipBytes(uint8_t** data, size_t& dataLength, size_t size)
{
    size_t bytesLeft = size - structFillPosition;
    if (bytesLeft <= dataLength)
    {
        *data += bytesLeft;
        dataLength -= bytesLeft;
        structFillPosition = 0;
        return true;
    }
    else
    {
        *data += dataLength;
        structFillPosition += dataLength;
        dataLength = 0;
        return false;
    }
}

void ImageDecoder::Begin(Image* image, bool dimensionsOnly)
{
    onlyDownloadDimensions = dimensionsOnly;
    state = ImageDecoder::Decoding;
    outputImage = image;
    outputImage->bpp = Platform::video->drawSurface->bpp == 1 ? 1 : 8;
}

void ImageDecoder::CalculateImageDimensions(int sourceWidth, int sourceHeight)
{
	VideoModeInfo* modeInfo = Platform::video->GetVideoModeInfo();

	if (modeInfo->aspectRatio != 100)
	{
		sourceHeight = ((long)sourceHeight * 100) / modeInfo->aspectRatio;
	}
	if (modeInfo->zoom != 100)
	{
		sourceWidth = ((long)modeInfo->zoom * sourceWidth) / 100;
		sourceHeight = ((long)modeInfo->zoom * sourceHeight) / 100;
	}

    if (sourceHeight < 1)
        sourceHeight = 1;
    if (sourceWidth < 1)
        sourceWidth = 1;

    int calculatedWidth = sourceWidth;
    int calculatedHeight = sourceHeight;

    if (outputImage->width != 0)
    {
        // Specified by layout
        calculatedWidth = outputImage->width;
        
        if (outputImage->height == 0)
        {
            calculatedHeight = ((long)sourceHeight * outputImage->width) / sourceWidth;
            if (calculatedHeight <= 0)
                calculatedHeight = 1;
        }
    }
    if (outputImage->height != 0)
    {
        // Specified by layout
        calculatedHeight = outputImage->height;

        if (outputImage->width == 0)
        {
            calculatedWidth = ((long)sourceWidth * outputImage->height) / sourceHeight;
            if (calculatedWidth <= 0)
                calculatedWidth = 1;
        }
    }

    outputImage->width = calculatedWidth;
    outputImage->height = calculatedHeight;
}
