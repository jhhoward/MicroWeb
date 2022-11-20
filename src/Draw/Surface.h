#ifndef _SURFACE_H_
#define _SURFACE_H_

#include "../Font.h"
struct Font;
class Image;

struct DrawContext
{
	DrawContext() {}
	DrawContext(int inClipLeft, int inClipTop, int inClipRight, int inClipBottom)
		: clipLeft(inClipLeft), clipTop(inClipTop), clipRight(inClipRight), clipBottom(inClipBottom)
	{

	}
	int clipLeft, clipTop, clipRight, clipBottom;
};

class DrawSurface
{
public:
	DrawSurface(int inWidth, int inHeight) : width(inWidth), height(inHeight) {}
	virtual void HLine(DrawContext& context, int x, int y, int count, uint8_t colour) = 0;
	virtual void VLine(DrawContext& context, int x, int y, int count, uint8_t colour) = 0;
	virtual void FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour) = 0;
	virtual void DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style = FontStyle::Regular) = 0;
	virtual void BlitImage(DrawContext& context, Image* image, int x, int y) = 0;

	int width, height;
};

#endif
