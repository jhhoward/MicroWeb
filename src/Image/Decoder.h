#ifndef _DECODER_H_
#define _DECODER_H_

#include <stdint.h>
#include <stddef.h>
struct Image;
class LinearAllocator;

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

	ImageDecoder() : outputImage(NULL) {}
	virtual void Begin(Image* image) = 0;
	virtual void Process(uint8_t* data, size_t dataLength) = 0;
	virtual State GetState() = 0;
	
	static ImageDecoder* Get();
	static ImageDecoder* Create(DecoderType type);

protected:
	Image* outputImage;
};



#endif
