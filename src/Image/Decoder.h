#ifndef _DECODER_H_
#define _DECODER_H_

#include <stdint.h>
struct Image;

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

	ImageDecoder() : outputImage(NULL) {}
	virtual void Begin(Image* image) = 0;
	virtual void Process(uint8_t* data, size_t dataLength) = 0;
	virtual State GetState() = 0;
	
protected:
	Image* outputImage;
};



#endif
