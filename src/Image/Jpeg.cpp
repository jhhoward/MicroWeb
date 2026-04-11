#include "Jpeg.h"
#include "Image.h"
#include "../Platform.h"
#include "../Memory/Memory.h"
#include <stdio.h>

#define PJPG_MAXCOMPSINSCAN 3
#ifdef _WIN32
#define DEBUG_MESSAGE(...) printf(__VA_ARGS__);
#else
#define DEBUG_MESSAGE(...)
#endif

#define PJPG_INLINE inline
#define PJPG_ARITH_SHIFT_RIGHT_N_16(x, n) ((x) >> (n))
#define PJPG_ARITH_SHIFT_RIGHT_8_L(x) ((x) >> 8)

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

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

const uint8_t DQT = 0xDB; // Define quantization table
const uint8_t DRI = 0xDD; // Define restart interval
const uint8_t DHT = 0xC4; // Define huffman table

const uint8_t SOI = 0xD8; // Start of Image
const uint8_t EOI = 0xD9; // End of Image

const uint8_t SOS = 0xDA;	// Start of scan


//----------------------------------------------------------------------------
// Winograd IDCT: 5 multiplies per row/col, up to 80 muls for the 2D IDCT

#define PJPG_DCT_SCALE_BITS 7

#define PJPG_DCT_SCALE (1U << PJPG_DCT_SCALE_BITS)

#define PJPG_DESCALE(x) PJPG_ARITH_SHIFT_RIGHT_N_16(((x) + (1 << (PJPG_DCT_SCALE_BITS - 1))), PJPG_DCT_SCALE_BITS)

#define PJPG_WFIX(x) ((x) * PJPG_DCT_SCALE + 0.5f)

#define PJPG_WINOGRAD_QUANT_SCALE_BITS 10

const uint8_t gWinogradQuant[] =
{
   128,  178,  178,  167,  246,  167,  151,  232,
   232,  151,  128,  209,  219,  209,  128,  101,
   178,  197,  197,  178,  101,   69,  139,  167,
   177,  167,  139,   69,   35,   96,  131,  151,
   151,  131,   96,   35,   49,   91,  118,  128,
   118,   91,   49,   46,   81,  101,  101,   81,
   46,   42,   69,   79,   69,   42,   35,   54,
   54,   35,   28,   37,   28,   19,   19,   10,
};

static const int8_t ZAG[] =
{
   0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63,
};

JpegDecoder::JpegDecoder()
	: internalState(ParseStartMarker)
	, restartInterval(0)
	, gValidHuffTables(0)
	, gValidQuantTables(0)
{
	bitBufferMask = 0x80;
	bitBufferStart = bitBufferEnd = 0;
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
			if (FillStruct(&data, dataLength, &marker.type, sizeof(marker.type)))
			{
				if (marker.type.highByte != 0xff || marker.type.lowByte != SOI)
				{
					DEBUG_MESSAGE("Unexpected marker at start of file!\n");
					state = ImageDecoder::Error;
					return;
				}
				internalState = ParseMarker;
			}
			break;
		case ParseMarker:
			if (FillStruct(&data, dataLength, &marker, sizeof(marker)))
			{
				if (marker.type.highByte != 0xff)
				{
					DEBUG_MESSAGE("Expected 0xff start of marker but got 0x%2x\n", marker.type.highByte);
					state = ImageDecoder::Error;
					return;
				}

				// The length includes the 16 bit length value so skip it
				if (marker.length < 2)
				{
					DEBUG_MESSAGE("Invalid marker length");
					state = ImageDecoder::Error;
					return;
				}
				marker.length = marker.length - 2;

				switch (marker.type.lowByte)
				{
				case EOI:
					state = ImageDecoder::Success;
					return;
				case SOF0:
				case SOF1:
				case SOF2:
				case SOF3:
				case SOF5:
				case SOF6:
				case SOF7:
				case SOF9:
				case SOF10:
				case SOF11:
				case SOF13:
				case SOF14:
				case SOF15:
					internalState = ParseStartOfFrame;
					break;
				case DHT:
					internalState = onlyDownloadDimensions ? SkipSegment : ParseHuffmanTable;
					break;
				case DRI:
					internalState = onlyDownloadDimensions ? SkipSegment : ParseRestartInterval;
					break;
				case DQT:
					internalState = onlyDownloadDimensions ? SkipSegment : ParseQuantizationTable;
					break;
				case SOS:
					internalState = ParseStartOfScan;
					break;
				default:
					internalState = SkipSegment;
					break;
				}
			}
			break;
		case SkipSegment:
			if(SkipBytes(&data, dataLength, marker.length))		
			{
				internalState = ParseMarker;
			}
			break;
		case ParseStartOfFrame:
			if (FillStruct(&data, dataLength, &frameHeader, sizeof(FrameHeader)))
			{
				if (outputImage->width == 0 || outputImage->height == 0)
				{
					CalculateImageDimensions(frameHeader.width, frameHeader.height);

					if (onlyDownloadDimensions)
					{
						state = ImageDecoder::Success;
						return;
					}
				}

				if (outputImage->width == 0 || outputImage->height == 0)
				{
					state = ImageDecoder::Error;
					return;
				}

				outputImage->bpp = 8;
				outputImage->pitch = outputImage->width;
				outputImage->lines = MemoryManager::pageBlockAllocator.Allocate(sizeof(MemBlockHandle) * outputImage->height);
				if (!outputImage->lines.IsAllocated())
				{
					// Allocation error
					DEBUG_MESSAGE("Could not allocate!\n");
					state = ImageDecoder::Error;
					return;
				}

				MemBlockHandle* lines = outputImage->lines.Get<MemBlockHandle*>();

				for (int j = 0; j < outputImage->height; j++)
				{
					lines[j] = MemoryManager::pageBlockAllocator.Allocate(outputImage->pitch);

					if (!lines[j].IsAllocated())
					{
						// Allocation error
						DEBUG_MESSAGE("Could not allocate!\n");
						outputImage->lines.type = MemBlockHandle::Unallocated;
						state = ImageDecoder::Error;
						return;
					}
				}

				outputImage->lines.Commit();
				for (int j = 0; j < outputImage->height; j++)
				{
					lines = outputImage->lines.Get<MemBlockHandle*>();
					MemBlockHandle line = lines[j];
					void* pixels = line.GetPtr();
					if (pixels)
					{
						//memset(pixels, TRANSPARENT_COLOUR_VALUE, outputImage->pitch);
						memset(pixels, 4, outputImage->pitch);
						line.Commit();
					}
				}

				gImageXSize = frameHeader.width;
				gImageYSize = frameHeader.height;
				gCompsInFrame = frameHeader.numComponents;

				uint8_t codingMode = marker.type.lowByte;

				if (codingMode != SOF0)
				{
					// Only baseline is supported
					DEBUG_MESSAGE("Unsupported coding mode: %2x\n", codingMode);
					state = ImageDecoder::Error;
					return;
				}

				if (gCompsInFrame > 3)
				{
					// Too many components
					DEBUG_MESSAGE("Too many components: %d\n", gCompsInFrame);
					state = ImageDecoder::Error;
					return;
				}

				marker.length = marker.length - sizeof(FrameHeader);
				if (marker.length != gCompsInFrame * 3)
				{
					// Invalid length
					DEBUG_MESSAGE("Invalid marker length: %d bytes remaining\n", (uint16_t)marker.length);
					state = ImageDecoder::Error;
					return;
				}

				counter = 0;
				internalState = ParseStartOfFrameComps;
			}
			break;

		case ParseStartOfFrameComps:
			if (FillStruct(&data, dataLength, gCompValues, 3))
			{
				gCompIdent[counter] = gCompValues[0];
				gCompHSamp[counter] = gCompValues[1] >> 4;
				gCompVSamp[counter] = gCompValues[1] & 0xf;
				gCompQuant[counter] = gCompValues[2];

				if (gCompQuant[counter] > 1)
				{
					// Unsupported quant table
					DEBUG_MESSAGE("Unsupported quant table\n");
					state = ImageDecoder::Error;
					return;
				}

				counter++;
				if (counter == gCompsInFrame)
				{
					if (!InitFrame())
					{
						state = ImageDecoder::Error;
						return;
					}
					internalState = ParseMarker;
				}
			}
			break;

		case ParseHuffmanTable:
			{
				if (marker.length == 0)
				{
					DEBUG_MESSAGE("Unexpected end of huffman table parsing\n");
					state = ImageDecoder::Error;
					return;
				}

				uint8_t huffmanTableInfo;
				if (FillStruct(&data, dataLength, &huffmanTableInfo, sizeof(huffmanTableInfo)))
				{
					marker.length = marker.length - 1;

					if (((huffmanTableInfo & 0xF) > 1) || ((huffmanTableInfo & 0xF0) > 0x10))
					{
						DEBUG_MESSAGE("Invalid huffman table data\n");
						state = ImageDecoder::Error;
						return;
					}

					huffmanTableIndex = ((huffmanTableInfo >> 3) & 2) + (huffmanTableInfo & 1);
					gValidHuffTables |= (1 << huffmanTableIndex);

					huffmanCount = 0;
					counter = 0;
					internalState = ParseHuffmanTableBits;
				}
			}
			break;
		case ParseHuffmanTableBits:
			{
				if (marker.length == 0)
				{
					DEBUG_MESSAGE("Unexpected end of huffman table parsing\n");
					state = ImageDecoder::Error;
					return;
				}
			
				uint8_t value;
				if (FillStruct(&data, dataLength, &value, sizeof(value)))
				{
					marker.length = marker.length - 1;
					huffmanBits[counter++] = value;
					huffmanCount += value;

					if (counter == 16)
					{
						if (huffmanCount > GetMaxHuffCodes(huffmanTableIndex))
						{
							DEBUG_MESSAGE("Invalid huffman table data\n");
							state = ImageDecoder::Error;
							return;
						}

						counter = 0;
						internalState = ParseHuffmanTableValues;
					}
				}
			}
			break;
		case ParseHuffmanTableValues:
			{
				if (marker.length == 0)
				{
					DEBUG_MESSAGE("Unexpected end of huffman table parsing\n");
					state = ImageDecoder::Error;
					return;
				}
			
				uint8_t value;

				if (FillStruct(&data, dataLength, &value, sizeof(value)))
				{
					marker.length = marker.length - 1;
					uint8_t* pHuffVal = GetHuffVal(huffmanTableIndex);
					pHuffVal[counter++] = value;

					if(counter == huffmanCount)
					{
						HuffCreate(huffmanBits, GetHuffTable(huffmanTableIndex));

						if (marker.length > 0)
						{
							internalState = ParseHuffmanTable;
						}
						else
						{
							internalState = ParseMarker;
						}
					}
				}
			}
			break;
		case ParseQuantizationTable:
			{
				uint8_t quantizationTableInfo;
				if (FillStruct(&data, dataLength, &quantizationTableInfo, sizeof(quantizationTableInfo)))
				{
					quantizationTableIndex = quantizationTableInfo & 0xf;
					quantizationTablePrecision = quantizationTableInfo >> 4;

					if (quantizationTableIndex > 1)
					{
						DEBUG_MESSAGE("Invalid quantization table index\n");
						state = ImageDecoder::Error;
						return;
					}

					gValidQuantTables |= (quantizationTableIndex ? 2 : 1);
					counter = 0;
					internalState = ParseQuantizationTableData;

					marker.length = marker.length - 1;
				}
			}
			break;
		case ParseQuantizationTableData:
			{
				if (quantizationTablePrecision)
				{
					// 16 bit
					if (marker.length <= 1)
					{
						DEBUG_MESSAGE("Not enough data for quantization table data\n");
						state = ImageDecoder::Error;
						return;
					}

					uint16_be value;
					if (FillStruct(&data, dataLength, &value, sizeof(value)))
					{
						if (quantizationTableIndex)
						{
							gQuant1[counter++] = value;
						}
						else
						{
							gQuant0[counter++] = value;
						}
						marker.length = marker.length - 2;
					}
				}
				else
				{
					// 8 bit
					if (marker.length == 0)
					{
						DEBUG_MESSAGE("Not enough data for quantization table data\n");
						state = ImageDecoder::Error;
						return;
					}

					uint8_t value;
					if (FillStruct(&data, dataLength, &value, sizeof(value)))
					{
						if (quantizationTableIndex)
						{
							gQuant1[counter++] = value;
						}
						else
						{
							gQuant0[counter++] = value;
						}
						marker.length = marker.length - 1;
					}
				}

				if (counter == 64)
				{
					CreateWinogradQuant(quantizationTableIndex ? gQuant1 : gQuant0);

					if (marker.length == 0)
					{
						internalState = ParseMarker;
					}
					else
					{
						internalState = ParseQuantizationTable;
					}
				}
			}
			break;
		case ParseRestartInterval:
			if (FillStruct(&data, dataLength, &restartInterval, sizeof(restartInterval)))
			{
				internalState = ParseMarker;
			}
			break;

		case ParseStartOfScan:
			if (FillStruct(&data, dataLength, &gCompsInScan, sizeof(gCompsInScan)))
			{
				marker.length = marker.length - 1;

				if ((marker.length != (gCompsInScan + gCompsInScan + 3)) || (gCompsInScan < 1) || (gCompsInScan > PJPG_MAXCOMPSINSCAN))
				{
					// Bad SOS length
					DEBUG_MESSAGE("Bad SOS length\n");
					state = ImageDecoder::Error;
					return;
				}

				counter = 0;
				internalState = ParseScanComps;
			}
			break;

		case ParseScanComps:
			if (counter < gCompsInScan)
			{
				if (FillStruct(&data, dataLength, gCompValues, 2))
				{
					marker.length = marker.length - 2;

					uint8_t cc = gCompValues[0];
					uint8_t c = gCompValues[1];
					uint8_t ci;

					for (ci = 0; ci < gCompsInFrame; ci++)
						if (cc == gCompIdent[ci])
							break;

					if (ci >= gCompsInFrame)
					{
						// PJPG_BAD_SOS_COMP_ID;
						DEBUG_MESSAGE("Bad SOS Comp index\n");
						state = ImageDecoder::Error;
						return;
					}

					gCompList[counter++] = ci;
					gCompDCTab[ci] = (c >> 4) & 15;
					gCompACTab[ci] = (c & 15);
				}
			}
			else if (SkipBytes(&data, dataLength, marker.length))
			{
				if (CheckHuffTables())
				{
					DEBUG_MESSAGE("Bad huff tables\n");
					state = ImageDecoder::Error;
					return;
				}

				if (CheckQuantTables())
				{
					DEBUG_MESSAGE("Bad quant tables\n");
					state = ImageDecoder::Error;
					return;
				}

				gLastDC[0] = 0;
				gLastDC[1] = 0;
				gLastDC[2] = 0;

				mcuX = mcuY = 0;

				if (restartInterval)
				{
					gRestartsLeft = restartInterval;
					gNextRestartNum = 0;
				}

				// TODO- important?
				//fixInBuffer();
				mcuBlock = 0;
				internalState = ParseMCU;
				mcuState = ProcessRestarts;
			}
			break; 

		case ParseMCU:
			while (state == ImageDecoder::Decoding)
			{
				FillBitBuffer(&data, dataLength);
				SaveBitBufferState();

				if (ProcessMCU())
				{
					if (mcuState == CompletedMCU)
					{
						mcuBlock++;
						if (mcuBlock == gMaxBlocksPerMCU)
						{
							BlitBlock();

							mcuBlock = 0;

							gNumMCUSRemainingX--;
							if (!gNumMCUSRemainingX)
							{
								gNumMCUSRemainingY--;
								if (gNumMCUSRemainingY > 0)
									gNumMCUSRemainingX = gMaxMCUSPerRow;
							}

							if ((!gNumMCUSRemainingX) && (!gNumMCUSRemainingY))
							{
								state = ImageDecoder::Success;
								return;
							}
							mcuState = ProcessRestarts;
						}
						else
						{
							mcuState = StartMCU;
						}
					}
				}
				else
				{
					//DEBUG_MESSAGE("Restoring bit buffer!\n");

					// Not enough bits in the buffer to process or otherwise failed
					RestoreBitBufferState();

					if (dataLength > 0)
					{
						// This shouldn't happen - we filled the bit buffer but there weren't enough bits to process
						// Probably means that our bit buffer isn't large enough
						DEBUG_MESSAGE("Bit buffer not large enough?\n");
						state = ImageDecoder::Error;
					}

					return;
				}
			}
			break;
		}
	}
}

// Multiply quantization matrix by the Winograd IDCT scale factors
void JpegDecoder::CreateWinogradQuant(int16_t* pQuant)
{
	uint8_t i;

	for (i = 0; i < 64; i++)
	{
		long x = pQuant[i];
		x *= gWinogradQuant[i];
		pQuant[i] = (int16_t)((x + (1 << (PJPG_WINOGRAD_QUANT_SCALE_BITS - PJPG_DCT_SCALE_BITS - 1))) >> (PJPG_WINOGRAD_QUANT_SCALE_BITS - PJPG_DCT_SCALE_BITS));
	}
}

uint16_t JpegDecoder::GetMaxHuffCodes(uint8_t index)
{
	return (index < 2) ? 12 : 255;
}

JpegDecoder::HuffTable* JpegDecoder::GetHuffTable(uint8_t index)
{
	// 0-1 = DC
	// 2-3 = AC
	switch (index)
	{
	case 0: return &gHuffTab0;
	case 1: return &gHuffTab1;
	case 2: return &gHuffTab2;
	case 3: return &gHuffTab3;
	default: return 0;
	}
}

uint8_t* JpegDecoder::GetHuffVal(uint8_t index)
{
	// 0-1 = DC
	// 2-3 = AC
	switch (index)
	{
	case 0: return gHuffVal0;
	case 1: return gHuffVal1;
	case 2: return gHuffVal2;
	case 3: return gHuffVal3;
	default: return 0;
	}
}

void JpegDecoder::HuffCreate(const uint8_t* pBits, HuffTable* pHuffTable)
{
	uint8_t i = 0;
	uint8_t j = 0;

	uint16_t code = 0;

	for (; ; )
	{
		uint8_t num = pBits[i];

		if (!num)
		{
			pHuffTable->mMinCode[i] = 0x0000;
			pHuffTable->mMaxCode[i] = 0xFFFF;
			pHuffTable->mValPtr[i] = 0;
		}
		else
		{
			pHuffTable->mMinCode[i] = code;
			pHuffTable->mMaxCode[i] = code + num - 1;
			pHuffTable->mValPtr[i] = j;

			j = (uint8_t)(j + num);

			code = (uint16_t)(code + num);
		}

		code <<= 1;

		i++;
		if (i > 15)
			break;
	}
}

uint8_t JpegDecoder::CheckHuffTables(void)
{
	uint8_t i;

	for (i = 0; i < gCompsInScan; i++)
	{
		uint8_t compDCTab = gCompDCTab[gCompList[i]];
		uint8_t compACTab = gCompACTab[gCompList[i]] + 2;

		if (((gValidHuffTables & (1 << compDCTab)) == 0) ||
			((gValidHuffTables & (1 << compACTab)) == 0))
			return 1;
	}

	return 0;
}

uint8_t JpegDecoder::CheckQuantTables(void)
{
	uint8_t i;

	for (i = 0; i < gCompsInScan; i++)
	{
		uint8_t compQuantMask = gCompQuant[gCompList[i]] ? 2 : 1;

		if ((gValidQuantTables & compQuantMask) == 0)
			return 1;
	}

	return 0;
}


void JpegDecoder::FillBitBuffer(uint8_t** data, size_t& length)
{
	while (length > 0)
	{
		uint8_t bitBufferNext = (bitBufferEnd + 1) & (JPEG_BIT_BUFFER_SIZE - 1);
		if (bitBufferNext != bitBufferStart)
		{
			bitBuffer[bitBufferEnd] = **data;
			(*data)++;
			bitBufferEnd = bitBufferNext;
			length--;
		}
		else break;
	}
}

bool JpegDecoder::GetBit(uint8_t& output)
{
	if (bitBufferStart != bitBufferEnd)
	{
		uint8_t bits = bitBuffer[bitBufferStart];

		if (bitBufferMask == 0x80 && bits == 0xff)
		{
			// Hit escape code
			uint8_t nextPosition = (bitBufferStart + 1) & (JPEG_BIT_BUFFER_SIZE - 1);
			if (nextPosition == bitBufferEnd)
			{
				// Escape code type not yet in buffer
				return false;
			}

			if (bitBuffer[nextPosition] != 0)
			{
				uint8_t nextNextPosition = (nextPosition + 1) & (JPEG_BIT_BUFFER_SIZE - 1);
				if (nextNextPosition == bitBufferEnd)
				{
					// Data after escape code not yet in buffer
					return false;
				}

				bitBufferStart = nextNextPosition;
				bits = bitBuffer[bitBufferStart];

				// TODO: handle this
				//DEBUG_MESSAGE("Unhandled escape code! %d\n", bitBuffer[nextPosition]);
				//return false;
			}
		}

		output = (bits & bitBufferMask) != 0 ? 1 : 0;
		bitBufferMask >>= 1;
		if (bitBufferMask == 0)
		{
			bitBufferMask = 0x80;
			bitBufferStart = (bitBufferStart + 1) & (JPEG_BIT_BUFFER_SIZE - 1);

			if (bits == 0xff)
			{
				bitBufferStart = (bitBufferStart + 1) & (JPEG_BIT_BUFFER_SIZE - 1);
			}
		}
		return true;
	}
	return false;
}

bool JpegDecoder::GetBits(uint8_t bits, uint16_t& output)
{
	output = 0;
	while (bits--)
	{
		uint8_t bit;

		if (!GetBit(bit))
		{
			return false;
		}
		
		output <<= 1;
		output |= bit;
	}

	return true;
}

void JpegDecoder::SaveBitBufferState()
{
	savedBitBufferMask = bitBufferMask;
	savedBitBufferStart = bitBufferStart;
}

void JpegDecoder::RestoreBitBufferState()
{
	bitBufferMask = savedBitBufferMask;
	bitBufferStart = savedBitBufferStart;
}

uint16_t JpegDecoder::GetExtendTest(uint8_t i)
{
	switch (i)
	{
	case 0: return 0;
	case 1: return 0x0001;
	case 2: return 0x0002;
	case 3: return 0x0004;
	case 4: return 0x0008;
	case 5: return 0x0010;
	case 6: return 0x0020;
	case 7: return 0x0040;
	case 8:  return 0x0080;
	case 9:  return 0x0100;
	case 10: return 0x0200;
	case 11: return 0x0400;
	case 12: return 0x0800;
	case 13: return 0x1000;
	case 14: return 0x2000;
	case 15: return 0x4000;
	default: return 0;
	}
}

int16_t JpegDecoder::GetExtendOffset(uint8_t i)
{
	switch (i)
	{
	case 0: return 0;
	case 1: return ((-1) << 1) + 1;
	case 2: return ((-1) << 2) + 1;
	case 3: return ((-1) << 3) + 1;
	case 4: return ((-1) << 4) + 1;
	case 5: return ((-1) << 5) + 1;
	case 6: return ((-1) << 6) + 1;
	case 7: return ((-1) << 7) + 1;
	case 8: return ((-1) << 8) + 1;
	case 9: return ((-1) << 9) + 1;
	case 10: return ((-1) << 10) + 1;
	case 11: return ((-1) << 11) + 1;
	case 12: return ((-1) << 12) + 1;
	case 13: return ((-1) << 13) + 1;
	case 14: return ((-1) << 14) + 1;
	case 15: return ((-1) << 15) + 1;
	default: return 0;
	}
};

int16_t JpegDecoder::HuffExtend(uint16_t x, uint8_t s)
{
	return ((x < GetExtendTest(s)) ? ((int16_t)x + GetExtendOffset(s)) : (int16_t)x);
}

bool JpegDecoder::HuffDecode(const HuffTable* pHuffTable, const uint8_t* pHuffVal, uint8_t& output)
{
	uint8_t i = 0;
	uint8_t j;
	uint16_t code;
	uint8_t bit;
		
	if (!GetBit(bit))
	{
		return false;
	}
	code = bit;

	// This func only reads a bit at a time, which on modern CPU's is not terribly efficient.
	// But on microcontrollers without strong integer shifting support this seems like a 
	// more reasonable approach.
	for (; ; )
	{
		uint16_t maxCode;

		if (i == 16)
		{
			output = 0;
			return true;
		}

		maxCode = pHuffTable->mMaxCode[i];
		if ((code <= maxCode) && (maxCode != 0xFFFF))
			break;

		i++;
		code <<= 1;

		if (!GetBit(bit))
		{
			return false;
		}
		code |= bit;
	}

	j = pHuffTable->mValPtr[i];
	j = (uint8_t)(j + (code - pHuffTable->mMinCode[i]));

	output = pHuffVal[j];
	return true;
}

bool JpegDecoder::FindIntervalMarker()
{
	uint8_t pos = bitBufferStart;

	while(pos != bitBufferEnd)
	{
		uint8_t next = (pos + 1) & (JPEG_BIT_BUFFER_SIZE - 1);

		if (bitBuffer[pos] == 0xff)
		{
			if (next == bitBufferEnd)
			{
				// Not enough in buffer
				return false;
			}

			if (bitBuffer[next] >= 0xD0 && bitBuffer[next] <= 0xD7)
			{
				// Skip over
				bitBufferStart = (next + 1) & (JPEG_BIT_BUFFER_SIZE - 1);
				return true;
			}
		}

		pos = next;
	}

	DEBUG_MESSAGE("Error: interval marker couldn't be found!");

	return false;
}

bool JpegDecoder::ProcessMCU()
{
	switch (mcuState)
	{
		case ProcessRestarts:
		{
			if (restartInterval)
			{
				if (gRestartsLeft == 0)
				{
					if (FindIntervalMarker())
					{
						// Reset each component's DC prediction values.
						gLastDC[0] = 0;
						gLastDC[1] = 0;
						gLastDC[2] = 0;

						gRestartsLeft = restartInterval;

						gNextRestartNum = (gNextRestartNum + 1) & 7;
					}
					else
					{
						return false;
					}
				}
				gRestartsLeft--;
			}
			mcuState = StartMCU;
		}
		break;
		case StartMCU:
		{
			//DEBUG_MESSAGE("Block %d X %d Y %d\n", mcuBlock, gNumMCUSRemainingX, gNumMCUSRemainingY);

			uint8_t componentID = gMCUOrg[mcuBlock];
			uint8_t compQuant = gCompQuant[componentID];
			uint8_t compDCTab = gCompDCTab[componentID];
			uint8_t numExtraBits;
			const int16_t* pQ = compQuant ? gQuant1 : gQuant0;
			uint16_t r, dc;

			uint8_t s;

			if (!HuffDecode(compDCTab ? &gHuffTab1 : &gHuffTab0, compDCTab ? gHuffVal1 : gHuffVal0, s))
			{
				return false;
			}

			r = 0;
			numExtraBits = s & 0xF;
			if (numExtraBits)
			{
				if (!GetBits(numExtraBits, r))
				{
					return false;
				}
			}
			dc = HuffExtend(r, s);

			dc = dc + gLastDC[componentID];
			gLastDC[componentID] = dc;

			//printf("%x %x\n", s, dc);

			gCoeffBuf[0] = dc * pQ[0];

			mcuCounter = 1;
			mcuState = DecodeMCU;
			break;
		}

		case DecodeMCU:
		{
			uint8_t componentID = gMCUOrg[mcuBlock];
			uint8_t compACTab = gCompACTab[componentID];
			uint8_t compQuant = gCompQuant[componentID];	
			const int16_t* pQ = compQuant ? gQuant1 : gQuant0;
			uint8_t numExtraBits;
			uint16_t extraBits;
			uint8_t s;

			if (!HuffDecode(compACTab ? &gHuffTab3 : &gHuffTab2, compACTab ? gHuffVal3 : gHuffVal2, s))
			{
				return false;
			}

			extraBits = 0;
			numExtraBits = s & 0xF;
			if (numExtraBits)
			{
				if (!GetBits(numExtraBits, extraBits))
				{
					return false;
				}
			}

			//printf("%x/%x ", s, extraBits);
			//if (s == 0x7 && extraBits == 0x3f)
			if (s == 0x57 && extraBits == 0x64)
			{
				//printf("");
			}

			uint16_t r = s >> 4;
			s &= 15;

			if (s)
			{
				int16_t ac;

				if (r)
				{
					if ((mcuCounter + r) > 63)
					{
						//return PJPG_DECODE_ERROR;
						DEBUG_MESSAGE("Error in decode\n");
						state = ImageDecoder::Error;
						return false;
					}

					while (r)
					{
						gCoeffBuf[ZAG[mcuCounter++]] = 0;
						r--;
					}
				}

				ac = HuffExtend(extraBits, s);

				gCoeffBuf[ZAG[mcuCounter]] = ac * pQ[mcuCounter];
			}
			else
			{
				if (r == 15)
				{
					if ((mcuCounter + 16) > 64)
					{
						DEBUG_MESSAGE("Error in decode\n");
						state = ImageDecoder::Error;
						return false;
					}

					for (r = 16; r > 0; r--)
						gCoeffBuf[ZAG[mcuCounter++]] = 0;

					mcuCounter--; // - 1 because the loop counter is k
				}
				else
				{
					//printf("#");
					mcuState = TransformMCU;
					return true;
				}
			}

			mcuCounter++;
			if (mcuCounter >= 64)
			{
				mcuState = TransformMCU;
			}
		}
		break;

		case TransformMCU:
		{
			while (mcuCounter < 64)
				gCoeffBuf[ZAG[mcuCounter++]] = 0;

			TransformBlock(mcuBlock);

			mcuState = CompletedMCU;

			break;
		}
	}
	return true;
}

bool JpegDecoder::InitFrame()
{
	if (gCompsInFrame == 1)
	{
		if ((gCompHSamp[0] != 1) || (gCompVSamp[0] != 1))
		{
			//return PJPG_UNSUPPORTED_SAMP_FACTORS;
			DEBUG_MESSAGE("Unsupported samp factors!\n");
			return false;
		}

		gScanType = PJPG_GRAYSCALE;

		gMaxBlocksPerMCU = 1;
		gMCUOrg[0] = 0;

		gMaxMCUXSize = 8;
		gMaxMCUYSize = 8;
	}
	else if (gCompsInFrame == 3)
	{
		if (((gCompHSamp[1] != 1) || (gCompVSamp[1] != 1)) ||
			((gCompHSamp[2] != 1) || (gCompVSamp[2] != 1)))
		{
			//return PJPG_UNSUPPORTED_SAMP_FACTORS;
			DEBUG_MESSAGE("Unsupported samp factors!\n");
			return false;
		}

		if ((gCompHSamp[0] == 1) && (gCompVSamp[0] == 1))
		{
			gScanType = PJPG_YH1V1;

			gMaxBlocksPerMCU = 3;
			gMCUOrg[0] = 0;
			gMCUOrg[1] = 1;
			gMCUOrg[2] = 2;

			gMaxMCUXSize = 8;
			gMaxMCUYSize = 8;
		}
		else if ((gCompHSamp[0] == 1) && (gCompVSamp[0] == 2))
		{
			gScanType = PJPG_YH1V2;

			gMaxBlocksPerMCU = 4;
			gMCUOrg[0] = 0;
			gMCUOrg[1] = 0;
			gMCUOrg[2] = 1;
			gMCUOrg[3] = 2;

			gMaxMCUXSize = 8;
			gMaxMCUYSize = 16;
		}
		else if ((gCompHSamp[0] == 2) && (gCompVSamp[0] == 1))
		{
			gScanType = PJPG_YH2V1;

			gMaxBlocksPerMCU = 4;
			gMCUOrg[0] = 0;
			gMCUOrg[1] = 0;
			gMCUOrg[2] = 1;
			gMCUOrg[3] = 2;

			gMaxMCUXSize = 16;
			gMaxMCUYSize = 8;
		}
		else if ((gCompHSamp[0] == 2) && (gCompVSamp[0] == 2))
		{
			gScanType = PJPG_YH2V2;

			gMaxBlocksPerMCU = 6;
			gMCUOrg[0] = 0;
			gMCUOrg[1] = 0;
			gMCUOrg[2] = 0;
			gMCUOrg[3] = 0;
			gMCUOrg[4] = 1;
			gMCUOrg[5] = 2;

			gMaxMCUXSize = 16;
			gMaxMCUYSize = 16;
		}
		else
		{
			//return PJPG_UNSUPPORTED_SAMP_FACTORS;
			DEBUG_MESSAGE("Unsupported samp factors!\n");
			return false;
		}
	}
	else
	{
		//return PJPG_UNSUPPORTED_COLORSPACE;
		DEBUG_MESSAGE("Unsupported colour space!\n");
		return false;
	}

	gMaxMCUSPerRow = (gImageXSize + (gMaxMCUXSize - 1)) >> ((gMaxMCUXSize == 8) ? 3 : 4);
	gMaxMCUSPerCol = (gImageYSize + (gMaxMCUYSize - 1)) >> ((gMaxMCUYSize == 8) ? 3 : 4);

	// This can overflow on large JPEG's.
	//gNumMCUSRemaining = gMaxMCUSPerRow * gMaxMCUSPerCol;
	gNumMCUSRemainingX = gMaxMCUSPerRow;
	gNumMCUSRemainingY = gMaxMCUSPerCol;

	return true;
}


// These multiply helper functions are the 4 types of signed multiplies needed by the Winograd IDCT.
// A smart C compiler will optimize them to use 16x8 = 24 bit muls, if not you may need to tweak
// these functions or drop to CPU specific inline assembly.

// 1/cos(4*pi/16)
// 362, 256+106
static PJPG_INLINE int16_t imul_b1_b3(int16_t w)
{
	long x = (w * 362L);
	x += 128L;
	return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

// 1/cos(6*pi/16)
// 669, 256+256+157
static PJPG_INLINE int16_t imul_b2(int16_t w)
{
	long x = (w * 669L);
	x += 128L;
	return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

// 1/cos(2*pi/16)
// 277, 256+21
static PJPG_INLINE int16_t imul_b4(int16_t w)
{
	long x = (w * 277L);
	x += 128L;
	return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

// 1/(cos(2*pi/16) + cos(6*pi/16))
// 196, 196
static PJPG_INLINE int16_t imul_b5(int16_t w)
{
	long x = (w * 196L);
	x += 128L;
	return (int16_t)(PJPG_ARITH_SHIFT_RIGHT_8_L(x));
}

static PJPG_INLINE uint8_t clamp(int16_t s)
{
	if ((uint16_t)s > 255U)
	{
		if (s < 0)
			return 0;
		else if (s > 255)
			return 255;
	}

	return (uint8_t)s;
}

void JpegDecoder::idctRows(void)
{
	uint8_t i;
	int16_t* pSrc = gCoeffBuf;

	for (i = 0; i < 8; i++)
	{
		if ((pSrc[1] | pSrc[2] | pSrc[3] | pSrc[4] | pSrc[5] | pSrc[6] | pSrc[7]) == 0)
		{
			// Short circuit the 1D IDCT if only the DC component is non-zero
			int16_t src0 = *pSrc;

			*(pSrc + 1) = src0;
			*(pSrc + 2) = src0;
			*(pSrc + 3) = src0;
			*(pSrc + 4) = src0;
			*(pSrc + 5) = src0;
			*(pSrc + 6) = src0;
			*(pSrc + 7) = src0;
		}
		else
		{
			int16_t src4 = *(pSrc + 5);
			int16_t src7 = *(pSrc + 3);
			int16_t x4 = src4 - src7;
			int16_t x7 = src4 + src7;

			int16_t src5 = *(pSrc + 1);
			int16_t src6 = *(pSrc + 7);
			int16_t x5 = src5 + src6;
			int16_t x6 = src5 - src6;

			int16_t tmp1 = imul_b5(x4 - x6);
			int16_t stg26 = imul_b4(x6) - tmp1;

			int16_t x24 = tmp1 - imul_b2(x4);

			int16_t x15 = x5 - x7;
			int16_t x17 = x5 + x7;

			int16_t tmp2 = stg26 - x17;
			int16_t tmp3 = imul_b1_b3(x15) - tmp2;
			int16_t x44 = tmp3 + x24;

			int16_t src0 = *(pSrc + 0);
			int16_t src1 = *(pSrc + 4);
			int16_t x30 = src0 + src1;
			int16_t x31 = src0 - src1;

			int16_t src2 = *(pSrc + 2);
			int16_t src3 = *(pSrc + 6);
			int16_t x12 = src2 - src3;
			int16_t x13 = src2 + src3;

			int16_t x32 = imul_b1_b3(x12) - x13;

			int16_t x40 = x30 + x13;
			int16_t x43 = x30 - x13;
			int16_t x41 = x31 + x32;
			int16_t x42 = x31 - x32;

			*(pSrc + 0) = x40 + x17;
			*(pSrc + 1) = x41 + tmp2;
			*(pSrc + 2) = x42 + tmp3;
			*(pSrc + 3) = x43 - x44;
			*(pSrc + 4) = x43 + x44;
			*(pSrc + 5) = x42 - tmp3;
			*(pSrc + 6) = x41 - tmp2;
			*(pSrc + 7) = x40 - x17;
		}

		pSrc += 8;
	}
}

void JpegDecoder::idctCols(void)
{
	uint8_t i;

	int16_t* pSrc = gCoeffBuf;

	for (i = 0; i < 8; i++)
	{
		if ((pSrc[1 * 8] | pSrc[2 * 8] | pSrc[3 * 8] | pSrc[4 * 8] | pSrc[5 * 8] | pSrc[6 * 8] | pSrc[7 * 8]) == 0)
		{
			// Short circuit the 1D IDCT if only the DC component is non-zero
			uint8_t c = clamp(PJPG_DESCALE(*pSrc) + 128);
			*(pSrc + 0 * 8) = c;
			*(pSrc + 1 * 8) = c;
			*(pSrc + 2 * 8) = c;
			*(pSrc + 3 * 8) = c;
			*(pSrc + 4 * 8) = c;
			*(pSrc + 5 * 8) = c;
			*(pSrc + 6 * 8) = c;
			*(pSrc + 7 * 8) = c;
		}
		else
		{
			int16_t src4 = *(pSrc + 5 * 8);
			int16_t src7 = *(pSrc + 3 * 8);
			int16_t x4 = src4 - src7;
			int16_t x7 = src4 + src7;

			int16_t src5 = *(pSrc + 1 * 8);
			int16_t src6 = *(pSrc + 7 * 8);
			int16_t x5 = src5 + src6;
			int16_t x6 = src5 - src6;

			int16_t tmp1 = imul_b5(x4 - x6);
			int16_t stg26 = imul_b4(x6) - tmp1;

			int16_t x24 = tmp1 - imul_b2(x4);

			int16_t x15 = x5 - x7;
			int16_t x17 = x5 + x7;

			int16_t tmp2 = stg26 - x17;
			int16_t tmp3 = imul_b1_b3(x15) - tmp2;
			int16_t x44 = tmp3 + x24;

			int16_t src0 = *(pSrc + 0 * 8);
			int16_t src1 = *(pSrc + 4 * 8);
			int16_t x30 = src0 + src1;
			int16_t x31 = src0 - src1;

			int16_t src2 = *(pSrc + 2 * 8);
			int16_t src3 = *(pSrc + 6 * 8);
			int16_t x12 = src2 - src3;
			int16_t x13 = src2 + src3;

			int16_t x32 = imul_b1_b3(x12) - x13;

			int16_t x40 = x30 + x13;
			int16_t x43 = x30 - x13;
			int16_t x41 = x31 + x32;
			int16_t x42 = x31 - x32;

			// descale, convert to unsigned and clamp to 8-bit
			*(pSrc + 0 * 8) = clamp(PJPG_DESCALE(x40 + x17) + 128);
			*(pSrc + 1 * 8) = clamp(PJPG_DESCALE(x41 + tmp2) + 128);
			*(pSrc + 2 * 8) = clamp(PJPG_DESCALE(x42 + tmp3) + 128);
			*(pSrc + 3 * 8) = clamp(PJPG_DESCALE(x43 - x44) + 128);
			*(pSrc + 4 * 8) = clamp(PJPG_DESCALE(x43 + x44) + 128);
			*(pSrc + 5 * 8) = clamp(PJPG_DESCALE(x42 - tmp3) + 128);
			*(pSrc + 6 * 8) = clamp(PJPG_DESCALE(x41 - tmp2) + 128);
			*(pSrc + 7 * 8) = clamp(PJPG_DESCALE(x40 - x17) + 128);
		}

		pSrc++;
	}
}

/*----------------------------------------------------------------------------*/
static PJPG_INLINE uint8_t addAndClamp(uint8_t a, int16_t b)
{
	b = a + b;

	if ((uint16_t)b > 255U)
	{
		if (b < 0)
			return 0;
		else if (b > 255)
			return 255;
	}

	return (uint8_t)b;
}
/*----------------------------------------------------------------------------*/
static PJPG_INLINE uint8_t subAndClamp(uint8_t a, int16_t b)
{
	b = a - b;

	if ((uint16_t)b > 255U)
	{
		if (b < 0)
			return 0;
		else if (b > 255)
			return 255;
	}

	return (uint8_t)b;
}
/*----------------------------------------------------------------------------*/
// 103/256
//R = Y + 1.402 (Cr-128)

// 88/256, 183/256
//G = Y - 0.34414 (Cb-128) - 0.71414 (Cr-128)

// 198/256
//B = Y + 1.772 (Cb-128)
/*----------------------------------------------------------------------------*/
// Cb upsample and accumulate, 4x4 to 8x8
void JpegDecoder::upsampleCb(uint8_t srcOfs, uint8_t dstOfs)
{
	// Cb - affects G and B
	uint8_t x, y;
	int16_t* pSrc = gCoeffBuf + srcOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	uint8_t* pDstB = gMCUBufB + dstOfs;
	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
		{
			uint8_t cb = (uint8_t)*pSrc++;
			int16_t cbG, cbB;

			cbG = ((cb * 88U) >> 8U) - 44U;
			pDstG[0] = subAndClamp(pDstG[0], cbG);
			pDstG[1] = subAndClamp(pDstG[1], cbG);
			pDstG[8] = subAndClamp(pDstG[8], cbG);
			pDstG[9] = subAndClamp(pDstG[9], cbG);

			cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
			pDstB[0] = addAndClamp(pDstB[0], cbB);
			pDstB[1] = addAndClamp(pDstB[1], cbB);
			pDstB[8] = addAndClamp(pDstB[8], cbB);
			pDstB[9] = addAndClamp(pDstB[9], cbB);

			pDstG += 2;
			pDstB += 2;
		}

		pSrc = pSrc - 4 + 8;
		pDstG = pDstG - 8 + 16;
		pDstB = pDstB - 8 + 16;
	}
}
/*----------------------------------------------------------------------------*/
// Cb upsample and accumulate, 4x8 to 8x8
void JpegDecoder::upsampleCbH(uint8_t srcOfs, uint8_t dstOfs)
{
	// Cb - affects G and B
	uint8_t x, y;
	int16_t* pSrc = gCoeffBuf + srcOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	uint8_t* pDstB = gMCUBufB + dstOfs;
	for (y = 0; y < 8; y++)
	{
		for (x = 0; x < 4; x++)
		{
			uint8_t cb = (uint8_t)*pSrc++;
			int16_t cbG, cbB;

			cbG = ((cb * 88U) >> 8U) - 44U;
			pDstG[0] = subAndClamp(pDstG[0], cbG);
			pDstG[1] = subAndClamp(pDstG[1], cbG);

			cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
			pDstB[0] = addAndClamp(pDstB[0], cbB);
			pDstB[1] = addAndClamp(pDstB[1], cbB);

			pDstG += 2;
			pDstB += 2;
		}

		pSrc = pSrc - 4 + 8;
	}
}
/*----------------------------------------------------------------------------*/
// Cb upsample and accumulate, 8x4 to 8x8
void JpegDecoder::upsampleCbV(uint8_t srcOfs, uint8_t dstOfs)
{
	// Cb - affects G and B
	uint8_t x, y;
	int16_t* pSrc = gCoeffBuf + srcOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	uint8_t* pDstB = gMCUBufB + dstOfs;
	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 8; x++)
		{
			uint8_t cb = (uint8_t)*pSrc++;
			int16_t cbG, cbB;

			cbG = ((cb * 88U) >> 8U) - 44U;
			pDstG[0] = subAndClamp(pDstG[0], cbG);
			pDstG[8] = subAndClamp(pDstG[8], cbG);

			cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
			pDstB[0] = addAndClamp(pDstB[0], cbB);
			pDstB[8] = addAndClamp(pDstB[8], cbB);

			++pDstG;
			++pDstB;
		}

		pDstG = pDstG - 8 + 16;
		pDstB = pDstB - 8 + 16;
	}
}
/*----------------------------------------------------------------------------*/
// 103/256
//R = Y + 1.402 (Cr-128)

// 88/256, 183/256
//G = Y - 0.34414 (Cb-128) - 0.71414 (Cr-128)

// 198/256
//B = Y + 1.772 (Cb-128)
/*----------------------------------------------------------------------------*/
// Cr upsample and accumulate, 4x4 to 8x8
void JpegDecoder::upsampleCr(uint8_t srcOfs, uint8_t dstOfs)
{
	// Cr - affects R and G
	uint8_t x, y;
	int16_t* pSrc = gCoeffBuf + srcOfs;
	uint8_t* pDstR = gMCUBufR + dstOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 4; x++)
		{
			uint8_t cr = (uint8_t)*pSrc++;
			int16_t crR, crG;

			crR = (cr + ((cr * 103U) >> 8U)) - 179;
			pDstR[0] = addAndClamp(pDstR[0], crR);
			pDstR[1] = addAndClamp(pDstR[1], crR);
			pDstR[8] = addAndClamp(pDstR[8], crR);
			pDstR[9] = addAndClamp(pDstR[9], crR);

			crG = ((cr * 183U) >> 8U) - 91;
			pDstG[0] = subAndClamp(pDstG[0], crG);
			pDstG[1] = subAndClamp(pDstG[1], crG);
			pDstG[8] = subAndClamp(pDstG[8], crG);
			pDstG[9] = subAndClamp(pDstG[9], crG);

			pDstR += 2;
			pDstG += 2;
		}

		pSrc = pSrc - 4 + 8;
		pDstR = pDstR - 8 + 16;
		pDstG = pDstG - 8 + 16;
	}
}
/*----------------------------------------------------------------------------*/
// Cr upsample and accumulate, 4x8 to 8x8
void JpegDecoder::upsampleCrH(uint8_t srcOfs, uint8_t dstOfs)
{
	// Cr - affects R and G
	uint8_t x, y;
	int16_t* pSrc = gCoeffBuf + srcOfs;
	uint8_t* pDstR = gMCUBufR + dstOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	for (y = 0; y < 8; y++)
	{
		for (x = 0; x < 4; x++)
		{
			uint8_t cr = (uint8_t)*pSrc++;
			int16_t crR, crG;

			crR = (cr + ((cr * 103U) >> 8U)) - 179;
			pDstR[0] = addAndClamp(pDstR[0], crR);
			pDstR[1] = addAndClamp(pDstR[1], crR);

			crG = ((cr * 183U) >> 8U) - 91;
			pDstG[0] = subAndClamp(pDstG[0], crG);
			pDstG[1] = subAndClamp(pDstG[1], crG);

			pDstR += 2;
			pDstG += 2;
		}

		pSrc = pSrc - 4 + 8;
	}
}
/*----------------------------------------------------------------------------*/
// Cr upsample and accumulate, 8x4 to 8x8
void JpegDecoder::upsampleCrV(uint8_t srcOfs, uint8_t dstOfs)
{
	// Cr - affects R and G
	uint8_t x, y;
	int16_t* pSrc = gCoeffBuf + srcOfs;
	uint8_t* pDstR = gMCUBufR + dstOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	for (y = 0; y < 4; y++)
	{
		for (x = 0; x < 8; x++)
		{
			uint8_t cr = (uint8_t)*pSrc++;
			int16_t crR, crG;

			crR = (cr + ((cr * 103U) >> 8U)) - 179;
			pDstR[0] = addAndClamp(pDstR[0], crR);
			pDstR[8] = addAndClamp(pDstR[8], crR);

			crG = ((cr * 183U) >> 8U) - 91;
			pDstG[0] = subAndClamp(pDstG[0], crG);
			pDstG[8] = subAndClamp(pDstG[8], crG);

			++pDstR;
			++pDstG;
		}

		pDstR = pDstR - 8 + 16;
		pDstG = pDstG - 8 + 16;
	}
}
/*----------------------------------------------------------------------------*/
// Convert Y to RGB
void JpegDecoder::copyY(uint8_t dstOfs)
{
	uint8_t i;
	uint8_t* pRDst = gMCUBufR + dstOfs;
	uint8_t* pGDst = gMCUBufG + dstOfs;
	uint8_t* pBDst = gMCUBufB + dstOfs;
	int16_t* pSrc = gCoeffBuf;

	for (i = 64; i > 0; i--)
	{
		uint8_t c = (uint8_t)*pSrc++;

		*pRDst++ = c;
		*pGDst++ = c;
		*pBDst++ = c;
	}
}
/*----------------------------------------------------------------------------*/
// Cb convert to RGB and accumulate
void JpegDecoder::convertCb(uint8_t dstOfs)
{
	uint8_t i;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	uint8_t* pDstB = gMCUBufB + dstOfs;
	int16_t* pSrc = gCoeffBuf;

	for (i = 64; i > 0; i--)
	{
		uint8_t cb = (uint8_t)*pSrc++;
		int16_t cbG, cbB;

		cbG = ((cb * 88U) >> 8U) - 44U;
		*pDstG++ = subAndClamp(pDstG[0], cbG);

		cbB = (cb + ((cb * 198U) >> 8U)) - 227U;
		*pDstB++ = addAndClamp(pDstB[0], cbB);
	}
}
/*----------------------------------------------------------------------------*/
// Cr convert to RGB and accumulate
void JpegDecoder::convertCr(uint8_t dstOfs)
{
	uint8_t i;
	uint8_t* pDstR = gMCUBufR + dstOfs;
	uint8_t* pDstG = gMCUBufG + dstOfs;
	int16_t* pSrc = gCoeffBuf;

	for (i = 64; i > 0; i--)
	{
		uint8_t cr = (uint8_t)*pSrc++;
		int16_t crR, crG;

		crR = (cr + ((cr * 103U) >> 8U)) - 179;
		*pDstR++ = addAndClamp(pDstR[0], crR);

		crG = ((cr * 183U) >> 8U) - 91;
		*pDstG++ = subAndClamp(pDstG[0], crG);
	}
}

void JpegDecoder::TransformBlock(uint8_t mcuBlock)
{
	idctRows();
	idctCols();

	switch (gScanType)
	{
	case PJPG_GRAYSCALE:
	{
		// MCU size: 1, 1 block per MCU
		copyY(0);
		break;
	}
	case PJPG_YH1V1:
	{
		// MCU size: 8x8, 3 blocks per MCU
		switch (mcuBlock)
		{
		case 0:
		{
			copyY(0);
			break;
		}
		case 1:
		{
			convertCb(0);
			break;
		}
		case 2:
		{
			convertCr(0);
			break;
		}
		}

		break;
	}
	case PJPG_YH1V2:
	{
		// MCU size: 8x16, 4 blocks per MCU
		switch (mcuBlock)
		{
		case 0:
		{
			copyY(0);
			break;
		}
		case 1:
		{
			copyY(128);
			break;
		}
		case 2:
		{
			upsampleCbV(0, 0);
			upsampleCbV(4 * 8, 128);
			break;
		}
		case 3:
		{
			upsampleCrV(0, 0);
			upsampleCrV(4 * 8, 128);
			break;
		}
		}

		break;
	}
	case PJPG_YH2V1:
	{
		// MCU size: 16x8, 4 blocks per MCU
		switch (mcuBlock)
		{
		case 0:
		{
			copyY(0);
			break;
		}
		case 1:
		{
			copyY(64);
			break;
		}
		case 2:
		{
			upsampleCbH(0, 0);
			upsampleCbH(4, 64);
			break;
		}
		case 3:
		{
			upsampleCrH(0, 0);
			upsampleCrH(4, 64);
			break;
		}
		}

		break;
	}
	case PJPG_YH2V2:
	{
		// MCU size: 16x16, 6 blocks per MCU
		switch (mcuBlock)
		{
		case 0:
		{
			copyY(0);
			break;
		}
		case 1:
		{
			copyY(64);
			break;
		}
		case 2:
		{
			copyY(128);
			break;
		}
		case 3:
		{
			copyY(192);
			break;
		}
		case 4:
		{
			upsampleCb(0, 0);
			upsampleCb(4, 64);
			upsampleCb(4 * 8, 128);
			upsampleCb(4 + 4 * 8, 192);
			break;
		}
		case 5:
		{
			upsampleCr(0, 0);
			upsampleCr(4, 64);
			upsampleCr(4 * 8, 128);
			upsampleCr(4 + 4 * 8, 192);
			break;
		}
		}

		break;
	}
	}
}

void JpegDecoder::BlitBlock()
{
	int row_blocks_per_mcu = gMaxMCUXSize >> 3;
	int col_blocks_per_mcu = gMaxMCUYSize >> 3;
	int outY = mcuY * gMaxMCUYSize;
	int outX = mcuX * gMaxMCUXSize;

	if (outputImage->bpp == 8)
	{
		if (outputImage->width == gImageXSize && outputImage->height == gImageYSize)
		{
			for (int j = 0; j < gMaxMCUYSize; j += 8)
			{
				const int by_limit = min(8, outputImage->height - (outY + j));

				for (int i = 0; i < gMaxMCUXSize; i += 8)
				{
					int src_ofs = (i * 8U) + (j * 16U);
					const uint8_t* pSrcR = gMCUBufR + src_ofs;
					const uint8_t* pSrcG = gMCUBufG + src_ofs;
					const uint8_t* pSrcB = gMCUBufB + src_ofs;

					const int bx_limit = min(8, outputImage->width - (outX + i));

					if (gScanType == PJPG_GRAYSCALE)
					{
						/*	int bx, by;
							for (by = 0; by < by_limit; by++)
							{
								uint8* pDst = pDst_block;

								for (bx = 0; bx < bx_limit; bx++)
									*pDst++ = *pSrcR++;

								pSrcR += (8 - bx_limit);

								pDst_block += row_pitch;
							}*/
					}
					else
					{
						int bx, by;
						for (by = 0; by < by_limit; by++)
						{
							MemBlockHandle* lines = outputImage->lines.Get<MemBlockHandle*>();
							MemBlockHandle lineOutput = lines[outY + by + j];
							uint8_t* output = lineOutput.Get<uint8_t*>();
							uint8_t* pDst = output + outX + i;

							for (bx = 0; bx < bx_limit; bx++)
							{
								uint8_t red = *pSrcR++;
								uint8_t green = *pSrcG++;
								uint8_t blue = *pSrcB++;

								*pDst++ = Platform::video->paletteLUT[RGB332(red, green, blue)];
							}

							pSrcR += (8 - bx_limit);
							pSrcG += (8 - bx_limit);
							pSrcB += (8 - bx_limit);

							lineOutput.Commit();
						}
					}
				}
			}
		}
		else
		{
			// Image needs resizing

			for (int j = 0; j < gMaxMCUYSize; j += 8)
			{
				int outY1 = ((outY + j) * (long)outputImage->height) / gImageYSize;
				int outY2 = ((outY + j + 8) * (long)outputImage->height) / gImageYSize;
				if (outY2 > outputImage->height)
					outY2 = outputImage->height;
				if (outY1 >= outY2)
					break;

				const int by_limit = min(8, outputImage->height - (outY + j));
				const int outH = outY2 - outY1;

				for (int i = 0; i < gMaxMCUXSize; i += 8)
				{
					int outX1 = ((outX + i) * (long)outputImage->width) / gImageXSize;
					int outX2 = ((outX + i + 8) * (long)outputImage->width) / gImageXSize;
					if (outX2 > outputImage->width)
						outX2 = outputImage->width;
					if (outX1 >= outX2)
						break;

					int src_ofs = (i * 8U) + (j * 16U);
					const uint8_t* pSrcR = gMCUBufR + src_ofs;
					const uint8_t* pSrcG = gMCUBufG + src_ofs;
					const uint8_t* pSrcB = gMCUBufB + src_ofs;

					const int bx_limit = min(8, outputImage->width - (outX + i));
					const int outW = outX2 - outX1;

					if (gScanType == PJPG_GRAYSCALE)
					{
						/*	int bx, by;
							for (by = 0; by < by_limit; by++)
							{
								uint8* pDst = pDst_block;

								for (bx = 0; bx < bx_limit; bx++)
									*pDst++ = *pSrcR++;

								pSrcR += (8 - bx_limit);

								pDst_block += row_pitch;
							}*/
					}
					else
					{
						int vertDiffA = 16;
						int vertDiffB = outH << 1;
						int vertD = vertDiffA - outH;

						for (int by = outY1; by < outY2; by++)
						{
							MemBlockHandle* lines = outputImage->lines.Get<MemBlockHandle*>();
							MemBlockHandle lineOutput = lines[by];
							uint8_t* pDst = lineOutput.Get<uint8_t*>() + outX1;

							int horizDiffA = 16;
							int horizDiffB = outW << 1;
							int D = horizDiffA - outW;
							int idx = 0;

							for (int bx = outX1; bx < outX2; bx++)
							{
								uint8_t red = pSrcR[idx];
								uint8_t green = pSrcG[idx];
								uint8_t blue = pSrcB[idx];
								*pDst++ = Platform::video->paletteLUT[RGB332(red, green, blue)];

								while (D > 0)
								{
									idx++;
									D -= horizDiffB;
								}
								D += horizDiffA;
							}

							lineOutput.Commit();

							while (vertD > 0)
							{
								pSrcR += 8;
								pSrcG += 8;
								pSrcB += 8;
								vertD -= vertDiffB;
							}
							vertD += vertDiffA;
						}
					}
				}
			}
		}
	}
	else
	{
		// TODO
	}

	mcuX++;
	if (mcuX == gMaxMCUSPerRow)
	{
		mcuX = 0;
		mcuY++;
	}
}
