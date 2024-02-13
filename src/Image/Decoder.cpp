#include <new>
#include "Gif.h"
#include "Decoder.h"

union 
{
	char gif[sizeof(GifDecoder)];
	//char png[sizeof(PngDecoder)];
	//char jpeg[sizeof(JpegDecoder)];
	char buffer[1];
} ImageDecoderUnion;


ImageDecoder* ImageDecoder::Get()
{
	return (ImageDecoder*)(ImageDecoderUnion.buffer);
}

ImageDecoder* ImageDecoder::Create(DecoderType type, LinearAllocator& allocator)
{
	return new (ImageDecoderUnion.buffer) GifDecoder(allocator);
}
