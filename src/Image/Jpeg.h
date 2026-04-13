#ifndef _JPEG_H_
#define _JPEG_H_

#include "Decoder.h"

#define JPEG_BIT_BUFFER_SIZE 256

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
		ParseMCU
	};

	enum MCUState
	{
		ProcessRestarts,
		StartMCU,
		DecodeMCU,
		TransformMCU,
		CompletedMCU
	};

	typedef enum
	{
		PJPG_GRAYSCALE,
		PJPG_YH1V1,
		PJPG_YH2V1,
		PJPG_YH1V2,
		PJPG_YH2V2
	} pjpeg_scan_type_t;

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
	bool isProgressive;

	uint8_t gValidHuffTables;
	uint8_t gValidQuantTables;

	// 128 bytes
	int16_t gCoeffBuf[8 * 8];

	// 8*8*4 bytes * 3 = 768
	uint8_t gMCUBufR[256];
	uint8_t gMCUBufG[256];
	uint8_t gMCUBufB[256];

	uint8_t gMCUBufY[256];
	uint8_t gMCUBufCr[256];
	uint8_t gMCUBufCb[256];

	// 256 bytes
	int16_t gQuant0[8 * 8];
	int16_t gQuant1[8 * 8];

	// 6 bytes
	int16_t gLastDC[3];

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

	uint16_t gImageXSize;
	uint16_t gImageYSize;
	uint8_t gCompsInFrame;
	uint8_t gCompValues[3];

	uint8_t gCompIdent[3];
	uint8_t gCompHSamp[3];
	uint8_t gCompVSamp[3];
	uint8_t gCompQuant[3];

	uint8_t gCompsInScan;
	uint8_t gCompList[3];
	uint8_t gCompDCTab[3]; // 0,1
	uint8_t gCompACTab[3]; // 0,1

	pjpeg_scan_type_t gScanType;

	uint8_t gMaxBlocksPerMCU;
	uint8_t gMaxMCUXSize;
	uint8_t gMaxMCUYSize;
	uint16_t gMaxMCUSPerRow;
	uint16_t gMaxMCUSPerCol;

	uint16_t gNumMCUSRemainingX, gNumMCUSRemainingY;

	uint8_t gMCUOrg[6];

	uint8_t bitBuffer[JPEG_BIT_BUFFER_SIZE];
	uint8_t bitBufferMask;
	uint8_t bitBufferStart;
	uint8_t bitBufferEnd;
	uint8_t savedBitBufferMask;
	uint8_t savedBitBufferStart;

	MCUState mcuState;
	uint8_t mcuCounter;
	uint8_t mcuBlock;
	int mcuX, mcuY;

	void CreateWinogradQuant(int16_t* pQuant);
	uint16_t GetMaxHuffCodes(uint8_t index);
	JpegDecoder::HuffTable* GetHuffTable(uint8_t index);
	uint8_t* GetHuffVal(uint8_t index);
	void HuffCreate(const uint8_t* pBits, HuffTable* pHuffTable);
	uint8_t CheckHuffTables(void);
	uint8_t CheckQuantTables(void);
	int16_t HuffExtend(uint16_t x, uint8_t s);
	bool HuffDecode(const HuffTable* pHuffTable, const uint8_t* pHuffVal, uint8_t& output);
	uint16_t GetExtendTest(uint8_t i);
	int16_t GetExtendOffset(uint8_t i);
	bool InitFrame();
	void TransformBlock(uint8_t mcuBlock);
	bool FindIntervalMarker();

	void FillBitBuffer(uint8_t** data, size_t& length);
	void SaveBitBufferState();
	void RestoreBitBufferState();
	bool GetBit(uint8_t& output);
	bool GetBits(uint8_t bits, uint16_t& output);

	bool ProcessMCU();
	void convertCr(uint8_t dstOfs);
	void convertCb(uint8_t dstOfs);
	void copyY(uint8_t dstOfs);
	void upsampleCrV(uint8_t srcOfs, uint8_t dstOfs);
	void upsampleCrH(uint8_t srcOfs, uint8_t dstOfs);
	void upsampleCr(uint8_t srcOfs, uint8_t dstOfs);
	void upsampleCbV(uint8_t srcOfs, uint8_t dstOfs);
	void upsampleCbH(uint8_t srcOfs, uint8_t dstOfs);
	void upsampleCb(uint8_t srcOfs, uint8_t dstOfs);
	void idctCols(void);
	void idctRows(void);

	void CopyValues(uint8_t* dest);
	void UpsampleValues(int srcOfs, uint8_t* dest);
	void UpsampleValuesH(int srcOfs, uint8_t* dest);
	void UpsampleValuesV(int srcOfs, uint8_t* dest);

	void BlitBlock();


};

#endif
