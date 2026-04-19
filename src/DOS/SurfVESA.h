#ifndef _SURFVESA_H_
#define _SURFVESA_H_

#include <stdint.h>
#include <dos.h>
#include "../Draw/Surface.h"

static void SetVESABank(uint16_t bank);
#pragma aux SetVESABank = \
	"xor bx, bx" \
	"mov ax, 0x4f05" \
	"int 0x10" \
	modify [ax bx] \
	parm [dx] 


extern uint16_t CurrentVESABank;

struct VESAPtr
{
	union
	{
		uint32_t linearAddress;
		struct
		{
			uint16_t offset;
			uint16_t bank;
		};
	};

	uint8_t* Get() const
	{
		if (CurrentVESABank != bank)
		{
			CurrentVESABank = bank;
			SetVESABank(bank);
		}
		return (uint8_t*)MK_FP(0xA000, offset);
	}

	void Set(uint8_t value) const
	{
		if (CurrentVESABank != bank)
		{
			CurrentVESABank = bank;
			SetVESABank(bank);
		}
		uint8_t* ptr = (uint8_t*) MK_FP(0xA000, offset);
		*ptr = value;
	}

	VESAPtr& operator++() 
	{
		linearAddress++;
		return *this;
	}

	VESAPtr operator++(int) 
	{
		VESAPtr temp = *this;
		linearAddress++;
		return temp;
	}

	VESAPtr& operator+=(uint16_t delta) 
	{
		linearAddress += delta;
		return *this;
	}

	VESAPtr operator+(uint16_t delta) const 
	{
		VESAPtr result;
		result.linearAddress = linearAddress + delta;
		return result;
	}
};

class DrawSurface_8BPP_VESA : public DrawSurface
{
public:
	DrawSurface_8BPP_VESA(int inWidth, int inHeight);

	virtual void Clear();
	virtual void HLine(DrawContext& context, int x, int y, int count, uint8_t colour);
	virtual void VLine(DrawContext& context, int x, int y, int count, uint8_t colour);
	virtual void FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour);
	virtual void DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style = FontStyle::Regular);
	virtual void BlitImage(DrawContext& context, Image* image, int x, int y);
	virtual void InvertRect(DrawContext& context, int x, int y, int width, int height);
	virtual void VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size);
	virtual void ScrollScreen(int top, int bottom, int width, int amount);
	virtual void DrawCursor(struct MouseCursorData* cursor, int x, int y);
	virtual void HideCursor();

private:
	void DrawScrollWidgetPart(uint8_t* pixels, int x, int y);

	VESAPtr* VESAlines;
	uint8_t* copyBuffer;

	uint8_t cursorBuffer[256];
	int cursorBufferX, cursorBufferY;
};

#endif
