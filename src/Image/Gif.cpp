#include "Gif.h"
#include "Image.h"
#include "../Platform.h"
#include "../Colour.h"
#include <stdio.h>

#define BLOCK_TYPE_EXTENSION_INTRODUCER 0x21
#define BLOCK_TYPE_IMAGE_DESCRIPTOR 0x2C
#define BLOCK_TYPE_TRAILER 0x3B

GifDecoder::GifDecoder(LinearAllocator& inAllocator) 
: allocator(inAllocator), state(ImageDecoder::Stopped)
{
}

void GifDecoder::Begin(Image* image)
{
	state = ImageDecoder::Decoding;
	outputImage = image;
	outputImage->bpp = 8;

	structFillPosition = 0;
	internalState = ParseHeader;
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
					if(memcmp(header.versionTag, "GIF89a", 6))
					{
						// Not a GIF89a
						state = ImageDecoder::Error;
						return;
					}
					
					outputImage->width = header.width;
					outputImage->height = header.height;
					outputImage->pitch = header.width;
					//outputImage->data = (uint8_t*) allocator.Alloc((header.width * header.height) >> 3);
					//outputImage->data = (uint8_t*) allocator.Alloc((header.width * header.height) * 3);
					outputImage->data = (uint8_t*)allocator.Alloc(outputImage->pitch * header.height); 
					if(!outputImage->data)
					{
						// Allocation error
						printf("Could not allocate!\n");
						state = ImageDecoder::Error;
						return;
					}
					
					backgroundColour = header.backgroundColour;
					
					printf("Image is %d x %d\n", outputImage->width, outputImage->height);
					
					if(header.fields & (1 << 7))
					{
						paletteSize = (1 << ((header.fields & 0x7) + 1));
						printf("Image has a palette of %d colours\n", (int) paletteSize);
						
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
					//printf("RGB index %d: %x %x %x\n", paletteIndex, (int)(rgb[0]), (int)(rgb[1]), (int)(rgb[2]));

					uint8_t grey = (uint8_t)(((uint16_t)rgb[0] * 76 + (uint16_t)rgb[1] * 150 + (uint16_t)rgb[2] * 30) >> 8);
					//palette[paletteIndex++] = grey;
					palette[paletteIndex * 3] = rgb[0];
					palette[paletteIndex * 3 + 1] = rgb[1];
					palette[paletteIndex * 3 + 2] = rgb[2];
					paletteIndex++;

					if(paletteIndex == paletteSize)
					{
						for (int n = 0; n < paletteSize; n++)
						{
							paletteLUT[n] = Platform::video->paletteLUT[RGB332(palette[n * 3], palette[n * 3 + 1], palette[n * 3 + 2])];
						}

						paletteLUT[header.backgroundColour] = Platform::video->colourScheme.pageColour;

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
					printf("Invalid block type: %x\n", (int)(blockType));
					state = ImageDecoder::Error;
					return;
				}
			}
			break;
			
			case ParseImageDescriptor:
			{
				if(FillStruct(&data, dataLength, &imageDescriptor, sizeof(ImageDescriptor)))
				{
					printf("Image: %d, %d %d, %d\n", imageDescriptor.x, imageDescriptor.y, imageDescriptor.width, imageDescriptor.height);
					
					drawX = drawY = 0;

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
					// TODO: use local colour table for something

					paletteIndex++;
					if (paletteIndex == localColourTableLength)
					{
						internalState = ParseLZWCodeSize;
					}
				}
			}
			break;

			case ParseLZWCodeSize:
			{
				lzwCodeSize = NextByte(&data, dataLength);
				printf("LZW code size: %d\n", lzwCodeSize);

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
				printf("Sub block size: %d bytes\n", imageSubBlockSize);
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
					//printf("-Data: %x\n", dataByte);
					
					for(int n = 0; n < 8; n++)
					{
						if(dataByte & (1 << n))
						{
							code |= (1 << codeBit);
						}
						codeBit++;
						
						if (codeBit == codeLength)
						{
							//printf("code: %x [len=%d]\n", code, codeLength);

							// Code complete
							if (code == clearCode)
							{
								printf("CLEAR\n");
								codeLength = resetCodeLength;
								ClearDictionary();
								prev = -1;
								codeBit = 0;
								code = 0;
								continue;
							}
							else if (code == stopCode)
							{
								printf("STOP\n");
								if (imageSubBlockSize)
								{
									printf("Malformed GIF\n");
									state = ImageDecoder::Error;
								}
								continue;
							}

							if (prev > -1 && codeLength <= 12)
							{
								if (code > dictionaryIndex)
								{
									printf("Error: code = %x, but dictionaryIndex = %x\n", code, dictionaryIndex);
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
									//printf("Code length: %d\n", codeLength);
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
										fprintf(stderr, "Stack overflow!");
										exit(-1);
									}

									stack[stackSize++] = dictionary[code].byte;
									//printf(" value: %x\n", dictionary[code].byte);

									code = dictionary[code].prev;
								}
								
								while(stackSize)
								{
									OutputPixel(stack[stackSize - 1]);
									stackSize--;
								}
							}
							
							codeBit = 0;
							code = 0;
						}
					}
				}
				else
				{
				//printf("--\n");
					internalState = ParseImageSubBlockSize;
				}
			}
			break;
			
			case ParseExtension:
			{
				if(FillStruct(&data, dataLength, &extensionHeader, sizeof(ExtensionHeader)))
				{
					printf("Extension: %x Size: %d\n", (int) extensionHeader.code, (int) extensionHeader.size);
					internalState = ParseExtensionContents;
				}
			}
			break;
			
			case ParseExtensionContents:
			{
				if(extensionHeader.size == 0)
				{
					internalState = ParseExtensionSubBlockSize;
				}
				else
				{
					NextByte(&data, dataLength);
					extensionHeader.size--;
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
				if(extensionSubBlockSize > 0)
				{
					NextByte(&data, dataLength);
					extensionSubBlockSize--;
				}
				else
				{
					internalState = ParseExtensionSubBlockSize;
				}
			}
			break;
		}
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
		outputImage->data[drawX++] = paletteLUT[pixelValue];
		//outputImage->data[drawX++] = palette[pixelValue * 3];
		//outputImage->data[drawX++] = palette[pixelValue * 3 + 1];
		//outputImage->data[drawX++] = palette[pixelValue * 3 + 2];
		//outputImage->data[drawX++] = pixelValue == header.backgroundColour ? 0 : 255;
	}
}
