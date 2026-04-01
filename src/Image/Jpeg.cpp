#include "Jpeg.h"
#include "Image.h"
#include "../Platform.h"

#define PJPG_MAXCOMPSINSCAN 3
#ifdef _WIN32
#define DEBUG_MESSAGE(...) printf(__VA_ARGS__);
#else
#define DEBUG_MESSAGE(...)
#endif

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

JpegDecoder::JpegDecoder()
	: internalState(ParseStartMarker)
	, restartInterval(0)
	, gValidHuffTables(0)
	, gValidQuantTables(0)
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

				uint8_t codingMode = marker.type.lowByte;

				if (codingMode != SOF0)
				{
					// Only baseline is supported
					DEBUG_MESSAGE("Unsupported coding mode: %2x\n", codingMode);
					state = ImageDecoder::Error;
					return;
				}

				if (frameHeader.numComponents > 3)
				{
					// Too many components
					DEBUG_MESSAGE("Too many components: %d\n", frameHeader.numComponents);
					state = ImageDecoder::Error;
					return;
				}

				marker.length = marker.length - sizeof(FrameHeader);
				if (marker.length != frameHeader.numComponents * 3)
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
				if (counter == frameHeader.numComponents)
				{
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

					for (ci = 0; ci < frameHeader.numComponents; ci++)
						if (cc == gCompIdent[ci])
							break;

					if (ci >= frameHeader.numComponents)
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

				if (restartInterval)
				{
					gRestartsLeft = restartInterval;
					gNextRestartNum = 0;
				}

				// TODO- important?
				//fixInBuffer();
				internalState = ParseMCUStart;
			}
			break; 

		case ParseMCUStart:
			if (restartInterval)
			{
				if (gRestartsLeft == 0)
				{
					// TODO: Handle restart intervals
					DEBUG_MESSAGE("Need to handle restart intervals!\n");
					state = ImageDecoder::Error;
					return;
				}
				gRestartsLeft--;
			}
			mcuBlock = 0;
			internalState = ParseMCU;
			break;

		case ParseMCU:
			state = ImageDecoder::Error;
			return;

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

