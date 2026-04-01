#ifndef _JPEG_H_
#define _JPEG_H_

#include "Decoder.h"

class JpegDecoder : public ImageDecoder
{
public:
	JpegDecoder();
	virtual void Process(uint8_t* data, size_t dataLength) override;

private:
	enum InternalState
	{
		ParseStartMarker,
		ParseMarker,
		SkipSegment,
		ParseStartOfFrame,
		ParseStartOfFrameComps,
		ParseQuantizationTable,
		ParseQuantizationTableData,
		ParseHuffmanTable,
		ParseHuffmanTableBits,
		ParseHuffmanTableValues,
		ParseRestartInterval,
		ParseStartOfScan,
		ParseScanComps,
		ParseScanParams,
		ParseMCUStart,
		ParseMCU
	};

#pragma pack(push, 1)
	struct FrameHeader
	{
		uint8_t bpp;
		uint16_be height;
		uint16_be width;
		uint8_t numComponents;
	};

	struct Marker
	{
		uint16_be type;
		uint16_be length;
	};

#pragma pack(pop)

	InternalState internalState;
	int16_t counter;

	Marker marker;

	uint16_be restartInterval;
	uint16_t gNextRestartNum;
	uint16_t gRestartsLeft;

	uint8_t quantizationTableIndex;
	uint8_t quantizationTablePrecision;

	FrameHeader frameHeader;

	uint8_t gValidHuffTables;
	uint8_t gValidQuantTables;

	// 256 bytes
	int16_t gQuant0[8 * 8];
	int16_t gQuant1[8 * 8];

	uint8_t huffmanTableIndex;
	uint8_t huffmanBits[16];
	uint16_t huffmanCount;

	typedef struct HuffTableT
	{
		uint16_t mMinCode[16];
		uint16_t mMaxCode[16];
		uint8_t mValPtr[16];
	} HuffTable;

	// DC - 192
	HuffTable gHuffTab0;
	uint8_t gHuffVal0[16];

	HuffTable gHuffTab1;
	uint8_t gHuffVal1[16];

	// AC - 672
	HuffTable gHuffTab2;
	uint8_t gHuffVal2[256];

	HuffTable gHuffTab3;
	uint8_t gHuffVal3[256];

	uint8_t gCompValues[3];

	uint8_t gCompIdent[3];
	uint8_t gCompHSamp[3];
	uint8_t gCompVSamp[3];
	uint8_t gCompQuant[3];

	uint8_t gCompsInScan;
	uint8_t gCompList[3];
	uint8_t gCompDCTab[3]; // 0,1
	uint8_t gCompACTab[3]; // 0,1

	// 6 bytes
	int16_t gLastDC[3];

	uint8_t mcuBlock;

	void CreateWinogradQuant(int16_t* pQuant);
	uint16_t GetMaxHuffCodes(uint8_t index);
	JpegDecoder::HuffTable* GetHuffTable(uint8_t index);
	uint8_t* GetHuffVal(uint8_t index);
	void HuffCreate(const uint8_t* pBits, HuffTable* pHuffTable);
	uint8_t CheckHuffTables(void);
	uint8_t CheckQuantTables(void);

	uint8_t ProcessRestart(void);

};

#endif
