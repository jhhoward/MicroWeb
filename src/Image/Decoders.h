#ifndef _DECODERS_H_
#define _DECODERS_H_

#include "Gif.h"
#include <new>

class LinearAllocator;

struct ImageDecoderUnion
{
	inline ImageDecoder* Get() { return (ImageDecoder*)(buffer); }

	template <typename T>
	void Create(LinearAllocator& allocator)
	{
		new (buffer) T(allocator);
	}

private:
	union
	{
		char gif[sizeof(GifDecoder)];
		//char png[sizeof(PngDecoder)];
		//char jpeg[sizeof(JpegDecoder)];
		char buffer[1];
	};

} ImageDecoders;

#endif
