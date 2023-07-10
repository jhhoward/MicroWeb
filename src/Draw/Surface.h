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
	DrawSurface* surface;
	int clipLeft, clipTop, clipRight, clipBottom;
	int drawOffsetX, drawOffsetY;
};

class DrawSurface
{
public:
	DrawSurface(int inWidth, int inHeight) : width(inWidth), height(inHeight) {}
	virtual void HLine(DrawContext& context, int x, int y, int count, uint8_t colour) = 0;
	virtual void VLine(DrawContext& context, int x, int y, int count, uint8_t colour) = 0;
	virtual void FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour) = 0;
	virtual void InvertRect(DrawContext& context, int x, int y, int width, int height) = 0;
	virtual void DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style = FontStyle::Regular) = 0;
	virtual void BlitImage(DrawContext& context, Image* image, int x, int y) = 0;

	int width, height;
};

#endif
