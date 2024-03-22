#ifndef _SURFACE_H_
#define _SURFACE_H_

#include "../Font.h"
struct Font;
struct Image;
class DrawSurface;

struct DrawContext
{
	DrawContext() {}
	DrawContext(DrawSurface* inSurface, int inClipLeft, int inClipTop, int inClipRight, int inClipBottom)
		: surface(inSurface), clipLeft(inClipLeft), clipTop(inClipTop), clipRight(inClipRight), clipBottom(inClipBottom), drawOffsetX(0), drawOffsetY(0)
	{

	}

	void Restrict(int left, int top, int right, int bottom)
	{
		left += drawOffsetX;
		right += drawOffsetX;
		top += drawOffsetY;
		bottom += drawOffsetY;
		if (left > clipLeft)
			clipLeft = left;
		if (right < clipRight)
			clipRight = right;
		if (top > clipTop)
			clipTop = top;
		if (bottom < clipBottom)
			clipBottom = bottom;
	}

	DrawSurface* surface;
	int clipLeft, clipTop, clipRight, clipBottom;
	int drawOffsetX, drawOffsetY;
};

class DrawSurface
{
public:
	DrawSurface(int inWidth, int inHeight) : width(inWidth), height(inHeight) {}
	virtual void Clear() = 0;
	virtual void HLine(DrawContext& context, int x, int y, int count, uint8_t colour) = 0;
	virtual void VLine(DrawContext& context, int x, int y, int count, uint8_t colour) = 0;
	virtual void FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour) = 0;
	virtual void InvertRect(DrawContext& context, int x, int y, int width, int height) = 0;
	virtual void DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style = FontStyle::Regular) = 0;
	virtual void BlitImage(DrawContext& context, Image* image, int x, int y) = 0;
	virtual void VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size) = 0;
	virtual void ScrollScreen(int top, int bottom, int width, int amount) {}

	uint8_t** lines;
	int width, height;
	uint8_t bpp;
};

#endif
