#include <memory.h>
#include <string.h>
#include "SurfVESA.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"
#include "../Colour.h"
#include "../Platform.h"

uint16_t CurrentVESABank = 0xffff;

DrawSurface_8BPP_VESA::DrawSurface_8BPP_VESA(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = nullptr;
	format = DrawSurface::Format_8BPP_VESA;
	VESAlines = new VESAPtr[height];
	copyBuffer = new uint8_t[width];

	uint32_t linearAddress = 0;

	for (int n = 0; n < height; n++)
	{
		VESAlines[n].linearAddress = linearAddress;
		linearAddress += width;
	}
}

void DrawSurface_8BPP_VESA::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	if (y < context.clipTop || y >= context.clipBottom)
	{
		return;
	}
	if (x < context.clipLeft)
	{
		count -= (context.clipLeft - x);
		x = context.clipLeft;
	}
	if (x + count >= context.clipRight)
	{
		count = context.clipRight - x;
	}
	if (count <= 0)
	{
		return;
	}
	
	VESAPtr VRAMptr = VESAlines[y] + x;
	uint16_t boundary = 65536UL - count;

	if (VRAMptr.offset > boundary)
	{
		int16_t first = 65536UL - VRAMptr.offset;
		int16_t second = count - first;
		memset(VRAMptr.Get(), colour, first);
		VRAMptr += first;
		memset(VRAMptr.Get(), colour, second);
	}
	else
	{
		memset(VRAMptr.Get(), colour, count);
	}
}

void DrawSurface_8BPP_VESA::VLine(DrawContext& context, int x, int y, int count, uint8_t colour)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	if (x >= context.clipRight || x < context.clipLeft)
	{
		return;
	}
	if (y < context.clipTop)
	{
		count -= (context.clipTop - y);
		y = context.clipTop;
	}
	if (y >= context.clipBottom)
	{
		return;
	}
	if (y + count > context.clipBottom)
	{
		count = context.clipBottom - y;
	}
	if (count <= 0)
	{
		return;
	}

	while (count--)
	{
		VESAPtr ptr = VESAlines[y] + x;
		ptr.Set(colour);
		y++;
	}
}

void DrawSurface_8BPP_VESA::FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	if (x < context.clipLeft)
	{
		width -= (context.clipLeft - x);
		x = context.clipLeft;
	}
	if (y < context.clipTop)
	{
		height -= (context.clipTop - y);
		y = context.clipTop;
	}
	if (x + width > context.clipRight)
	{
		width = context.clipRight - x;
	}
	if (y + height > context.clipBottom)
	{
		height = context.clipBottom - y;
	}
	if (width <= 0 || height <= 0)
	{
		return;
	}

	uint16_t boundary = 65536UL - width;

	while (height)
	{
		VESAPtr VRAMptr = VESAlines[y] + x;

		if (VRAMptr.offset > boundary)
		{
			int16_t first = 65536UL - VRAMptr.offset;
			int16_t second = width - first;
			memset(VRAMptr.Get(), colour, first);
			VRAMptr += first;
			memset(VRAMptr.Get(), colour, second);
		}
		else
		{
			memset(VRAMptr.Get(), colour, width);
		}

		height--;
		y++;
	}
}

void DrawSurface_8BPP_VESA::DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	int startX = x;
	uint8_t lastLine = font->glyphHeight;

	if (x >= context.clipRight)
	{
		return;
	}
	if (y >= context.clipBottom)
	{
		return;
	}
	if (y + lastLine > context.clipBottom)
	{
		lastLine = (uint8_t)(context.clipBottom - y);
	}
	if (y + lastLine < context.clipTop)
	{
		return;
	}

	uint8_t firstLine = 0;
	if (y < context.clipTop)
	{
		firstLine += context.clipTop - y;
		y += firstLine;
	}

	uint8_t bold = (style & FontStyle::Bold) ? 1 : 0;

	while (*text)
	{
		unsigned char c = (unsigned char) *text++;

		if (c < 32)
		{
			continue;
		}

		int index = c - 32;
		uint8_t glyphWidth = font->glyphs[index].width;
		uint8_t glyphTop = font->glyphs[index].top;
		uint8_t glyphBottom = font->glyphs[index].bottom;
		uint8_t glyphWidthBytes = (glyphWidth + 7) >> 3;
		uint8_t* glyphData = font->glyphData + font->glyphs[index].offset;
		int outY = y;

		if (glyphBottom > lastLine - 1)
		{
			glyphBottom = lastLine - 1;
		}
		if (firstLine < glyphTop)
		{
			outY += glyphTop - firstLine;
		}
		else if (firstLine > glyphTop)
		{
			glyphData += ((firstLine - glyphTop) * glyphWidthBytes);
			glyphTop = firstLine;
		}

		if (glyphWidth == 0)
		{
			continue;
		}

		glyphWidth += bold;

		if (x + glyphWidth > context.clipRight)
		{
			break;
		}

		VESAPtr VRAMptr = VESAlines[outY] + x;

		if (x >= 0)
		{
			for (uint8_t j = glyphTop; j <= glyphBottom; j++)
			{
				if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
				{
					++VRAMptr;
				}

				uint8_t boldCarry = 0;

				for (uint8_t i = 0; i < glyphWidthBytes; i++)
				{
					uint8_t glyphPixels = *glyphData++;

					if (bold)
					{
						if (boldCarry)
						{
							boldCarry = glyphPixels & 1;
							glyphPixels |= (glyphPixels >> 1) | 0x80;
						}
						else
						{
							boldCarry = glyphPixels & 1;
							glyphPixels |= (glyphPixels >> 1);
						}
					}

					for (uint8_t k = 0; k < 8; k++)
					{
						if (glyphPixels & (0x80 >> k))
						{
							VRAMptr.Set(colour);
						}
						++VRAMptr;
					}
				}

				outY++;
				VRAMptr = VESAlines[outY] + x;
			}
		}

		x += glyphWidth;

		//if (x >= context.clipRight)
		//{
		//	break;
		//}
	}

	if ((style & FontStyle::Underline) && y - firstLine + font->glyphHeight - 1 < context.clipBottom)
	{
		HLine(context, startX - context.drawOffsetX, y - firstLine + font->glyphHeight - 1 - context.drawOffsetY, x - startX, colour);
	}
}

void DrawSurface_8BPP_VESA::BlitImage(DrawContext& context, Image* image, int x, int y)
{
	if (!image->lines.IsAllocated())
		return;

	x += context.drawOffsetX;
	y += context.drawOffsetY;

	int srcWidth = image->width;
	int srcHeight = image->height;
	int srcPitch = image->pitch;
	int srcX = 0;
	int srcY = 0;

	// Calculate the destination width and height to copy, considering clipping region
	int destWidth = srcWidth;
	int destHeight = srcHeight;

	if (x < context.clipLeft)
	{
		srcX += (context.clipLeft - x);
		destWidth -= (context.clipLeft - x);
		x = context.clipLeft;
	}

	if (x + destWidth > context.clipRight)
	{
		destWidth = context.clipRight - x;
	}

	if (y < context.clipTop)
	{
		srcY += (context.clipTop - y);
		destHeight -= (context.clipTop - y);
		y = context.clipTop;
	}

	if (y + destHeight > context.clipBottom)
	{
		destHeight = context.clipBottom - y;
	}

	if (destWidth <= 0 || destHeight <= 0)
	{
		return; // Nothing to draw if fully outside the clipping region.
	}

	if (image->bpp == 8)
	{
		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
			MemBlockHandle imageLine = imageLines[srcY + j];
			uint8_t* src = imageLine.Get<uint8_t*>() + srcX;
			VESAPtr destRow = VESAlines[y + j] + x;

			for (int i = 0; i < destWidth; i++)
			{
				uint8_t pixel = *src++;

				if (pixel != TRANSPARENT_COLOUR_VALUE)
				{
					destRow.Set(pixel);
				}
				++destRow++;
			}
		}
	}
	else if (image->bpp == 1)
	{
		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
			MemBlockHandle imageLine = imageLines[srcY + j];
			uint8_t* src = imageLine.Get<uint8_t*>() + (srcX >> 3);
			uint8_t srcMask = 0x80 >> (srcX & 7);
			VESAPtr destRow = VESAlines[y + j] + x;
			uint8_t black = 0;
			uint8_t white = 0xf;
			uint8_t buffer = *src++;

			for (int i = 0; i < destWidth; i++)
			{
				if (buffer & srcMask)
				{
					destRow.Set(white);
					++destRow;
				}
				else
				{
					destRow.Set(black);
					++destRow;
				}
				srcMask >>= 1;
				if (!srcMask)
				{
					srcMask = 0x80;
					buffer = *src++;
				}
			}
		}
	}
}


void DrawSurface_8BPP_VESA::InvertRect(DrawContext& context, int x, int y, int width, int height)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	if (x < context.clipLeft)
	{
		width -= (context.clipLeft - x);
		x = context.clipLeft;
	}
	if (y < context.clipTop)
	{
		height -= (context.clipTop - y);
		y = context.clipTop;
	}
	if (x + width > context.clipRight)
	{
		width = context.clipRight - x;
	}
	if (y + height > context.clipBottom)
	{
		height = context.clipBottom - y;
	}
	if (width <= 0 || height <= 0)
	{
		return;
	}

	while (height)
	{
		VESAPtr VRAMptr = VESAlines[y] + x;
		int count = width;

		while (count--)
		{	
			uint8_t* ptr = VRAMptr.Get();
			*ptr ^= 0xff;
			++VRAMptr;
		}

		height--;
		y++;
	}
}

static uint8_t edge[16] =
{
	0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0
};

static uint8_t widgetEdge[16] =
{
	0x0,0xf,0xf,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xf,0xf,0x0
};

static uint8_t inner[16] =
{
	0x0,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0x0
};

static uint8_t widgetInner[16] =
{
	0x0,0xf,0x0,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0x0,0xf,0x0
};

static uint8_t grab[16] =
{
	0x0,0xf,0x0,0xf,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xf,0x0,0xf,0x0
};

void DrawSurface_8BPP_VESA::DrawScrollWidgetPart(uint8_t* pixels, int x, int y)
{
	VESAPtr ptr = VESAlines[y] + x;

	if (ptr.offset <= (65536L - 16))
	{
		memcpy(ptr.Get(), pixels, 16);
	}
	else
	{
		for (int n = 0; n < 16; n++)
		{
			ptr.Set(*pixels++);
			++ptr;
		}
	}
}

void DrawSurface_8BPP_VESA::VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;
	int startY = y;

	const int grabSize = 7;
	const int minWidgetSize = grabSize + 4;
	const int widgetPaddingSize = size - minWidgetSize;
	int topPaddingSize = widgetPaddingSize >> 1;
	int bottomPaddingSize = widgetPaddingSize - topPaddingSize;
	const uint16_t edge = 0;
	int bottomSpacing = height - position - size;

	while (position--)
	{
		DrawScrollWidgetPart(inner, x, y++);
	}

	DrawScrollWidgetPart(inner, x, y++);
	DrawScrollWidgetPart(widgetEdge, x, y++);

	while (topPaddingSize--)
	{
		DrawScrollWidgetPart(widgetInner, x, y++);
	}

	DrawScrollWidgetPart(widgetInner, x, y++);
	DrawScrollWidgetPart(grab, x, y++);
	DrawScrollWidgetPart(widgetInner, x, y++);
	DrawScrollWidgetPart(grab, x, y++);
	DrawScrollWidgetPart(widgetInner, x, y++);
	DrawScrollWidgetPart(grab, x, y++);
	DrawScrollWidgetPart(widgetInner, x, y++);

	while (bottomPaddingSize--)
	{
		DrawScrollWidgetPart(widgetInner, x, y++);
	}

	DrawScrollWidgetPart(widgetEdge, x, y++);
	DrawScrollWidgetPart(inner, x, y++);

	while (bottomSpacing--)
	{
		DrawScrollWidgetPart(inner, x, y++);
	}
}

void DrawSurface_8BPP_VESA::Clear()
{
	uint8_t colour = Platform::video->colourScheme.pageColour;

	for (int y = 0; y < height; y++)
	{
		VESAPtr VRAMptr = VESAlines[y];
		uint16_t boundary = 65536UL - width;

		if (VRAMptr.offset > boundary)
		{
			int16_t first = 65536UL - VRAMptr.offset;
			int16_t second = width - first;
			memset(VRAMptr.Get(), colour, first);
			VRAMptr += first;
			memset(VRAMptr.Get(), colour, second);
		}
		else
		{
			memset(VRAMptr.Get(), colour, width);
		}

		VRAMptr += width;
	}
}

void DrawSurface_8BPP_VESA::ScrollScreen(int top, int bottom, int width, int amount)
{
	uint16_t boundary = 65536UL - width;

	if (amount > 0)
	{
		for (int y = top; y < bottom; y++)
		{
			VESAPtr src = VESAlines[y + amount];
			VESAPtr dest = VESAlines[y];

			if (src.offset > boundary)
			{
				// Crosses a bank
				uint16_t diff = 65536UL - src.offset;
				memcpy(copyBuffer, src.Get(), diff);
				src += diff;
				memcpy(copyBuffer + diff, src.Get(), width - diff);
			}
			else
			{
				memcpy(copyBuffer, src.Get(), width);
			}

			if (dest.offset > boundary)
			{
				// Crosses a bank
				uint16_t diff = 65536UL - dest.offset;
				memcpy(dest.Get(), copyBuffer, diff);
				dest += diff;
				memcpy(dest.Get(), copyBuffer + diff, width - diff);
			}
			else
			{
				memcpy(dest.Get(), copyBuffer, width);
			}
		}
	}
	else if (amount < 0)
	{
		for (int y = bottom - 1; y >= top; y--)
		{
			VESAPtr src = VESAlines[y + amount];
			VESAPtr dest = VESAlines[y];

			if (src.offset > boundary)
			{
				// Crosses a bank
				uint16_t diff = 65536UL - src.offset;
				memcpy(copyBuffer, src.Get(), diff);
				src += diff;
				memcpy(copyBuffer + diff, src.Get(), width - diff);
			}
			else
			{
				memcpy(copyBuffer, src.Get(), width);
			}

			if (dest.offset > boundary)
			{
				// Crosses a bank
				uint16_t diff = 65536UL - dest.offset;
				memcpy(dest.Get(), copyBuffer, diff);
				dest += diff;
				memcpy(dest.Get(), copyBuffer + diff, width - diff);
			}
			else
			{
				memcpy(dest.Get(), copyBuffer, width);
			}
		}
	}
}
