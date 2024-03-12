#include "Gif.h"
#include "Image.h"
#include "../Platform.h"
#include "../Colour.h"
#include "../Memory/Memory.h"
#include "../Page.h"
#include "../App.h"
#include "../Draw/Surface.h"
#include <stdio.h>

#ifdef _WIN32
#define DEBUG_MESSAGE(...) printf(__VA_ARGS__);
#else
#define DEBUG_MESSAGE(...)
#endif

#define BLOCK_TYPE_EXTENSION_INTRODUCER 0x21
#define BLOCK_TYPE_IMAGE_DESCRIPTOR 0x2C
#define BLOCK_TYPE_TRAILER 0x3B
#define BLOCK_TYPE_GRAPHIC_CONTROL_EXTENSION 0xF9

GifDecoder::GifDecoder() 
: state(ImageDecoder::Stopped)
{
}

void GifDecoder::Begin(Image* image)
{
	state = ImageDecoder::Decoding;
	outputImage = image;
	outputImage->bpp = Platform::video->drawSurface->bpp == 1 ? 1 : 8;

	structFillPosition = 0;
	internalState = ParseHeader;
	lineBufferSkipCount = 0;
	transparentColourIndex = -1;
}

void GifDecoder::Process(uint8_t* data, size_t dataLength)
{
	if(state != ImageDecoder::Decoding)
	{
		return;
	}
	
	while(dataLength > 0)
	{
		switch(internalState)
		{
			case ParseHeader:
			{
				if(FillStruct(&data, dataLength, &header, sizeof(Header)))
				{
					// Header structure is complete
					if(memcmp(header.versionTag, "GIF89a", 6) && memcmp(header.versionTag, "GIF87a", 6))
					{
						// Not a GIF89a
						state = ImageDecoder::Error;
						return;
					}

					// If the image width is wider than the buffer, we will skip every N pixels
					lineBufferDivider = 1;
					while (header.width / lineBufferDivider > GIF_LINE_BUFFER_MAX_SIZE)
					{
						lineBufferDivider++;
					}
					
					if (outputImage->width == 0 && outputImage->height == 0)
					{
						int width = header.width;
						int height = header.height;
						Platform::video->ScaleImageDimensions(width, height);

						outputImage->width = width;
						outputImage->height = height;
					}

					if (outputImage->bpp == 1)
					{
						outputImage->pitch = (outputImage->width + 7) / 8;
					}
					else // 8bpp
					{
						outputImage->pitch = outputImage->width;
					}

					outputImage->lines = (MemBlockHandle*)MemoryManager::pageAllocator.Allocate(sizeof(MemBlockHandle) * outputImage->height);
					if(!outputImage->lines)
					{
						// Allocation error
						DEBUG_MESSAGE("Could not allocate!\n");
						state = ImageDecoder::Error;
						return;
					}

					for (int j = 0; j < outputImage->height; j++)
					{
						outputImage->lines[j] = MemoryManager::pageBlockAllocator.Allocate(outputImage->pitch);
						if (!outputImage->lines[j].IsAllocated())
						{
							// Allocation error
							DEBUG_MESSAGE("Could not allocate!\n");
							outputImage->lines = nullptr;
							state = ImageDecoder::Error;
							return;
						}
						void* pixels = outputImage->lines[j].GetPtr();
						if (pixels)
						{
							memset(pixels, TRANSPARENT_COLOUR_VALUE, outputImage->pitch);
							outputImage->lines[j].Commit();
						}
					}
					
					backgroundColour = header.backgroundColour;
					
					DEBUG_MESSAGE("Image is %d x %d\n", outputImage->width, outputImage->height);
					
					if(header.fields & (1 << 7))
					{
						paletteSize = (1 << ((header.fields & 0x7) + 1));
						DEBUG_MESSAGE("Image has a palette of %d colours\n", (int) paletteSize);
						
						internalState = ParsePalette;
						paletteIndex = 0;
					}
					else
					{
						internalState = ParseDataBlock;
					}
				}
			}
			break;
			
			case ParsePalette:
			{
				if(FillStruct(&data, dataLength, rgb, 3))
				{
					//DEBUG_MESSAGE("RGB index %d: %x %x %x\n", paletteIndex, (int)(rgb[0]), (int)(rgb[1]), (int)(rgb[2]));

					//uint8_t grey = (uint8_t)(((uint16_t)rgb[0] * 76 + (uint16_t)rgb[1] * 150 + (uint16_t)rgb[2] * 30) >> 8);
					//palette[paletteIndex++] = grey;
					palette[paletteIndex * 3] = rgb[0];
					palette[paletteIndex * 3 + 1] = rgb[1];
					palette[paletteIndex * 3 + 2] = rgb[2];
					paletteIndex++;

					if(paletteIndex == paletteSize)
					{
						if (outputImage->bpp == 8)
						{
							for (int n = 0; n < paletteSize; n++)
							{
								paletteLUT[n] = Platform::video->paletteLUT[RGB332(palette[n * 3], palette[n * 3 + 1], palette[n * 3 + 2])];
							}
						}
						else
						{
							for (int n = 0; n < paletteSize; n++)
							{
								paletteLUT[n] = RGB_TO_GREY(palette[n * 3], palette[n * 3 + 1], palette[n * 3 + 2]);
							}
						}

						if (transparentColourIndex >= 0)
						{
							paletteLUT[transparentColourIndex] = TRANSPARENT_COLOUR_VALUE;
						}

						internalState = ParseDataBlock;
					}
				}
			}
			break;
			
			case ParseDataBlock:
			{
				uint8_t blockType = NextByte(&data, dataLength);

				switch(blockType)
				{
					case BLOCK_TYPE_IMAGE_DESCRIPTOR:
					internalState = ParseImageDescriptor;
					break;
					
					case BLOCK_TYPE_TRAILER:
					// End of GIF
					state = ImageDecoder::Success;
					return;
					
					case BLOCK_TYPE_EXTENSION_INTRODUCER:
					internalState = ParseExtension;
					break;
					
					default:
						DEBUG_MESSAGE("Invalid block type: %x\n", (int)(blockType));
					state = ImageDecoder::Error;
					return;
				}
			}
			break;
			
			case ParseImageDescriptor:
			{
				if(FillStruct(&data, dataLength, &imageDescriptor, sizeof(ImageDescriptor)))
				{
					DEBUG_MESSAGE("Image: %d, %d %d, %d\n", imageDescriptor.x, imageDescriptor.y, imageDescriptor.width, imageDescriptor.height);
					
					drawX = drawY = 0;
					outputLine = 0;

					linesProcessed = 0;
					lineBufferSize = 0;
					lineBufferFlushCount = 0;

					if (imageDescriptor.fields & 0x80)
					{
						internalState = ParseLocalColourTable;
						paletteIndex = 0;
						localColourTableLength = 1 << ((imageDescriptor.fields & 7) + 1);
					}
					else
					{
						internalState = ParseLZWCodeSize;
					}
				}
			}
			break;

			case ParseLocalColourTable:
			{
				if (FillStruct(&data, dataLength, &rgb, 3))
				{
					palette[paletteIndex * 3] = rgb[0];
					palette[paletteIndex * 3 + 1] = rgb[1];
					palette[paletteIndex * 3 + 2] = rgb[2];

					paletteIndex++;

					if (paletteIndex == localColourTableLength)
					{
						if (outputImage->bpp == 8)
						{
							for (int n = 0; n < localColourTableLength; n++)
							{
								paletteLUT[n] = Platform::video->paletteLUT[RGB332(palette[n * 3], palette[n * 3 + 1], palette[n * 3 + 2])];
							}
						}
						else
						{
							for (int n = 0; n < localColourTableLength; n++)
							{
								paletteLUT[n] = RGB_TO_GREY(palette[n * 3], palette[n * 3 + 1], palette[n * 3 + 2]);
							}
						}

						if (transparentColourIndex >= 0)
						{
							paletteLUT[transparentColourIndex] = TRANSPARENT_COLOUR_VALUE;
						}

						internalState = ParseLZWCodeSize;
					}
				}
			}
			break;

			case ParseLZWCodeSize:
			{
				lzwCodeSize = NextByte(&data, dataLength);
				DEBUG_MESSAGE("LZW code size: %d\n", lzwCodeSize);

				// Init LZW vars
				code = 0;
				clearCode = 1 << lzwCodeSize;
				stopCode = clearCode + 1;
				codeLength = resetCodeLength = lzwCodeSize + 1;
				codeBit = 0;
				prev = -1;
				ClearDictionary();

				internalState = ParseImageSubBlockSize;
			}
			break;
			
			case ParseImageSubBlockSize:
			{
				imageSubBlockSize = NextByte(&data, dataLength);
				DEBUG_MESSAGE("Sub block size: %d bytes\n", imageSubBlockSize);
				if(imageSubBlockSize)
				{
					internalState = ParseImageSubBlock;
				}
				else
				{
					internalState = ParseDataBlock;

					// HACK: Finish decoding after first frame
					state = ImageDecoder::Success;
					return;
				}
			}
			break;
			
			case ParseImageSubBlock:
			{
				if(imageSubBlockSize)
				{
					imageSubBlockSize--;
					uint8_t dataByte = NextByte(&data, dataLength);
					//DEBUG_MESSAGE("-Data: %x\n", dataByte);
					
					for(int n = 0; n < 8; n++)
					{
						if(dataByte & (1 << n))
						{
							code |= (1 << codeBit);
						}
						codeBit++;
						
						if (codeBit == codeLength)
						{
							//DEBUG_MESSAGE("code: %x [len=%d]\n", code, codeLength);

							// Code complete
							if (code == clearCode)
							{
								DEBUG_MESSAGE("CLEAR\n");
								codeLength = resetCodeLength;
								ClearDictionary();
								prev = -1;
								codeBit = 0;
								code = 0;
								continue;
							}
							else if (code == stopCode)
							{
								DEBUG_MESSAGE("STOP\n");
								if (imageSubBlockSize)
								{
									DEBUG_MESSAGE("Malformed GIF\n");
									// FIXME
									//state = ImageDecoder::Success;
									//return;
									//state = ImageDecoder::Error;
								}
								continue;
							}

							if (prev > -1 && codeLength <= 12)
							{
								if (code > dictionaryIndex)
								{
									DEBUG_MESSAGE("Error: code = %x, but dictionaryIndex = %x\n", code, dictionaryIndex);
									state = ImageDecoder::Error;
									return;
								}

								int ptr = (code == dictionaryIndex) ? prev : code;
								while (dictionary[ptr].prev != -1)
								{
									ptr = dictionary[ptr].prev;
								}
								dictionary[dictionaryIndex].byte = dictionary[ptr].byte;

								dictionary[dictionaryIndex].prev = prev;
								if (prev != -1)
								{
									dictionary[dictionaryIndex].len = dictionary[prev].len + 1;
								}
								else
								{
									dictionary[dictionaryIndex].len = 1;
								}

								dictionaryIndex++;

								if(dictionaryIndex == (1 << (codeLength)) && codeLength < 12)
								{
									codeLength++;
									//DEBUG_MESSAGE("Code length: %d\n", codeLength);
								}
							}

							prev = code;
							
							{
								uint8_t stack[1024];
								int stackSize = 0;
								
								while(code != -1)
								{
									if (stackSize >= 1024)
									{
										DEBUG_MESSAGE("Stack overflow!");
										exit(-1);
									}

									stack[stackSize++] = dictionary[code].byte;
									//DEBUG_MESSAGE(" value: %x\n", dictionary[code].byte);

									code = dictionary[code].prev;
								}
								
								while (stackSize)
								{
									if (lineBufferSkipCount == lineBufferDivider - 1)
									{
										lineBuffer[lineBufferSize++] = stack[stackSize - 1];
										lineBufferSkipCount = 0;
									}
									else
									{
										lineBufferSkipCount++;
									}

									lineBufferFlushCount++;
									stackSize--;

									if (lineBufferFlushCount == imageDescriptor.width)
									{
										ProcessLineBuffer();
										lineBufferSize = 0;
										lineBufferFlushCount = 0;
									}
								}
							}
							
							codeBit = 0;
							code = 0;
						}
					}
				}
				else
				{
				//DEBUG_MESSAGE("--\n");
					internalState = ParseImageSubBlockSize;
				}
			}
			break;
			
			case ParseExtension:
			{
				if(FillStruct(&data, dataLength, &extensionHeader, sizeof(ExtensionHeader)))
				{
					DEBUG_MESSAGE("Extension: %x Size: %d\n", (int) extensionHeader.code, (int) extensionHeader.size);

					if (extensionHeader.code == BLOCK_TYPE_GRAPHIC_CONTROL_EXTENSION)
					{
						internalState = ParseGraphicControlExtension;
					}
					else
					{
						internalState = ParseExtensionContents;
					}
				}
			}
			break;
			
			case ParseExtensionContents:
			{
				if(SkipBytes(&data, dataLength, extensionHeader.size))
				{
					internalState = ParseExtensionSubBlockSize;
				}
			}
			break;

			case ParseGraphicControlExtension:
			{
				if (FillStruct(&data, dataLength, &graphicControlExtension, sizeof(GraphicControlExtension)))
				{
					if (graphicControlExtension.packedFields & 1)
					{
						transparentColourIndex = graphicControlExtension.transparentColourIndex;
						paletteLUT[transparentColourIndex] = TRANSPARENT_COLOUR_VALUE;
					}
					internalState = ParseExtensionSubBlockSize;
				}
			}
			break;
			
			case ParseExtensionSubBlockSize:
			{
				extensionSubBlockSize = NextByte(&data, dataLength);
				if(extensionSubBlockSize > 0)
				{
					internalState = ParseExtensionSubBlock;
				}
				else
				{
					internalState = ParseDataBlock;
				}
			}
			break;
			
			case ParseExtensionSubBlock:
			{
				if(SkipBytes(&data, dataLength, extensionSubBlockSize))
				{
					internalState = ParseExtensionSubBlockSize;
				}
			}
			break;
		}
	}
}

bool GifDecoder::SkipBytes(uint8_t** data, size_t& dataLength, size_t size)
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


bool GifDecoder::FillStruct(uint8_t** data, size_t& dataLength, void* dest, size_t size)
{
	size_t bytesLeft = size - structFillPosition;
	if(bytesLeft <= dataLength)
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

void GifDecoder::ClearDictionary()
{
	for(dictionaryIndex = 0; dictionaryIndex < (1 << lzwCodeSize); dictionaryIndex++)
	{
		dictionary[dictionaryIndex].byte = (uint8_t) dictionaryIndex;
		dictionary[dictionaryIndex].prev = -1;
		dictionary[dictionaryIndex].len = 1;
	}
	
	dictionaryIndex += 2;
}

void GifDecoder::OutputPixel(uint8_t pixelValue)
{	
	//printf(" value: %x\n", pixelValue);
	/*if(pixelValue == header.backgroundColour)
	{
		outputImage->data[drawX++] = 0xff;
		outputImage->data[drawX++] = 0x00;
		outputImage->data[drawX++] = 0xff;
		outputImage->data[drawX++] = 0xff;
	}
	else*/
	{
		outputImage->lines[outputLine].Get<uint8_t>()[drawX] = paletteLUT[pixelValue];
		outputImage->lines[outputLine].Commit();
		drawX++;

		if (drawX == imageDescriptor.width)
		{
			drawX = 0;
			drawY++;
			if (imageDescriptor.fields & GIF_INTERLACE_BIT)
			{
				outputLine = CalculateLineIndex(drawY);
			}
			else
			{
				outputLine = drawY;
			}

			if (outputLine >= imageDescriptor.height)
			{
				outputLine = imageDescriptor.height - 1;
			}

		}
		//outputImage->data[drawX++] = palette[pixelValue * 3];
		//outputImage->data[drawX++] = palette[pixelValue * 3 + 1];
		//outputImage->data[drawX++] = palette[pixelValue * 3 + 2];
		//outputImage->data[drawX++] = pixelValue == header.backgroundColour ? 0 : 255;
	}
}

// Compute output index of y-th input line, in frame of height h. 
int GifDecoder::CalculateLineIndex(int y)
{
	int p; /* number of lines in current pass */

	p = (header.height - 1) / 8 + 1;
	if (y < p) /* pass 1 */
		return y * 8;
	y -= p;
	p = (header.height - 5) / 8 + 1;
	if (y < p) /* pass 2 */
		return y * 8 + 4;
	y -= p;
	p = (header.height - 3) / 4 + 1;
	if (y < p) /* pass 3 */
		return y * 4 + 2;
	y -= p;
	/* pass 4 */
	return y * 2 + 1;
}

void GifDecoder::ProcessLineBuffer()
{
	int outputY = linesProcessed;
	
	if (imageDescriptor.fields & GIF_INTERLACE_BIT)
	{
		outputY = CalculateLineIndex(linesProcessed);
	}

	if (outputImage->height == header.height)
	{
		EmitLine(outputY);
	}
	else
	{
		int first = outputY * outputImage->height / header.height;
		int last = (outputY + 1) * outputImage->height / header.height;

		for (int y = first; y < last; y++)
		{
			EmitLine(y);
		}
	}

	linesProcessed++;
}

void GifDecoder::EmitLine(int y)
{
	uint8_t* output = outputImage->lines[y].Get<uint8_t>();
	
	if (outputImage->bpp == 8)
	{
		bool useColourDithering = true;

		if (useColourDithering)
		{
			const int8_t* ditherPattern = colourDitherMatrix + 4 * (y & 3);
			int ditherIndex = 0;

			if (outputImage->width == lineBufferSize)
			{
				for (int i = 0; i < lineBufferSize; i++)
				{
					int8_t offset = ditherPattern[ditherIndex];
					ditherIndex = (ditherIndex + 1) & 3;

					if (lineBuffer[i] == transparentColourIndex)
					{
						output[i] = TRANSPARENT_COLOUR_VALUE;
						continue;
					}

					int index = lineBuffer[i] * 3;
					int red = palette[index] + offset;
					if (red > 255)
						red = 255;
					else if (red < 0)
						red = 0;
					int green = palette[index + 1] + offset;
					if (green > 255)
						green = 255;
					else if (green < 0)
						green = 0;
					int blue = palette[index + 2] + offset;
					if (blue > 255)
						blue = 255;
					else if (blue < 0)
						blue = 0;

					output[i] = Platform::video->paletteLUT[RGB332(red, green, blue)];
				}
			}
			else
			{
				int dy = lineBufferSize;
				int dx = outputImage->width;
				int D = 2 * dy - dx;
				int x = 0;

				for (int i = 0; i < outputImage->width; i++)
				{
					int offset = ditherPattern[ditherIndex];
					ditherIndex = (ditherIndex + 1) & 3;

					if (lineBuffer[x] == transparentColourIndex)
					{
						output[i] = TRANSPARENT_COLOUR_VALUE;
					}
					else
					{
						int index = lineBuffer[x] * 3;
						int red = palette[index] + offset;
						if (red > 255)
							red = 255;
						else if (red < 0)
							red = 0;
						int green = palette[index + 1] + offset;
						if (green > 255)
							green = 255;
						else if (green < 0)
							green = 0;
						int blue = palette[index + 2] + offset;
						if (blue > 255)
							blue = 255;
						else if (blue < 0)
							blue = 0;

						output[i] = Platform::video->paletteLUT[RGB332(red, green, blue)];
					}

					while (D > 0)
					{
						x++;
						D -= 2 * dx;
					}
					D += 2 * dy;
				}
			}

		}
		else
		{
			if (outputImage->width == lineBufferSize)
			{
				for (int i = 0; i < lineBufferSize; i++)
				{
					output[i] = paletteLUT[lineBuffer[i]];
				}
			}
			else
			{
				int dy = lineBufferSize;
				int dx = outputImage->width;
				int D = 2 * dy - dx;
				int x = 0;

				for (int i = 0; i < outputImage->width; i++)
				{
					output[i] = paletteLUT[lineBuffer[x]];
					while (D > 0)
					{
						x++;
						D -= 2 * dx;
					}
					D += 2 * dy;
				}
			}
		}
	}
	else
	{
		const uint8_t* ditherPattern = greyDitherMatrix + 16 * (y & 15);
		uint8_t buffer = 0;
		uint8_t mask = 0x80;
		int ditherIndex = 0;

		if (outputImage->width == lineBufferSize)
		{
			for (int i = 0; i < lineBufferSize; i++)
			{
				uint8_t value = paletteLUT[lineBuffer[i]];
				uint8_t threshold = ditherPattern[ditherIndex];

				if (value > threshold)
				{
					buffer |= mask;
				}

				ditherIndex = (ditherIndex + 1) & 15;

				mask >>= 1;
				if (!mask)
				{
					*output++ = buffer;
					buffer = 0;
					mask = 0x80;
				}
			}

			if (mask != 0x80)
			{
				*output = buffer;
			}
		}
		else
		{
			
			int dy = lineBufferSize;
			int dx = outputImage->width;
			int D = 2 * dy - dx;
			int x = 0;

			for (int i = 0; i < outputImage->width; i++)
			{
				uint8_t value = paletteLUT[lineBuffer[x]];
				uint8_t threshold = ditherPattern[ditherIndex];

				if (value > threshold)
				{
					buffer |= mask;
				}

				ditherIndex = (ditherIndex + 1) & 15;

				mask >>= 1;
				if (!mask)
				{
					*output++ = buffer;
					buffer = 0;
					mask = 0x80;
				}

				while (D > 0)
				{
					x++;
					D -= 2 * dx;
				}
				D += 2 * dy;
			}
			
			if (mask != 0x80)
			{
				*output = buffer;
			}
		}
	}

	outputImage->lines[y].Commit();
}

