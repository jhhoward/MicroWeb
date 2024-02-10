#ifndef _SURF8BPP_H_
#define _SURF8BPP_H_

#include <stdint.h>
#include "Surface.h"

class DrawSurface_8BPP : public DrawSurface
{
public:
	DrawSurface_8BPP(int inWidth, int inHeight);

	virtual void Clear();
	virtual void HLine(DrawContext& context, int x, int y, int count, uint8_t colour);
	virtual void VLine(DrawContext& context, int x, int y, int count, uint8_t colour);
	virtual void FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour);
	virtual void DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style = FontStyle::Regular);
	virtual void BlitImage(DrawContext& context, Image* image, int x, int y);
	virtual void InvertRect(DrawContext& context, int x, int y, int width, int height);
	virtual void VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size);

	uint8_t** lines;
};

#endif
