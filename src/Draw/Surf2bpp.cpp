#include <memory.h>
#include "Surf2BPP.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"

static uint8_t bitmaskTable[] =
{
	0xc0, 0x30, 0x0c, 0x03
};

DrawSurface_2BPP::DrawSurface_2BPP(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
	bpp = 2;
}

void DrawSurface_2BPP::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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
	if (count < 0)
	{
		return;
	}

	uint8_t* VRAMptr = lines[y];
	VRAMptr += (x >> 2);

	uint8_t data = *VRAMptr;
	uint8_t mask = bitmaskTable[x & 3];

	while (count--)
	{
		data = (data & (~mask)) | (colour & mask);
		x++;
		mask >>= 2;
		if (!mask)
		{
			*VRAMptr++ = data;
			while (count > 4)
			{
				*VRAMptr++ = colour;
				count -= 4;
			}
			mask = 0xc0;
			data = *VRAMptr;
		}
	}

	*VRAMptr = data;
}

void DrawSurface_2BPP::VLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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
	if (y + count >= context.clipBottom)
	{
		count = context.clipBottom - 1 - y;
	}
	if (count <= 0)
	{
		return;
	}

	uint8_t mask = bitmaskTable[x & 3];
	uint8_t andMask = ~mask;
	uint8_t orMask = mask & colour;
	int index = x >> 2;

	while (count--)
	{
		(lines[y])[index] = ((lines[y])[index] & andMask) | orMask;
		y++;
	}
}

void DrawSurface_2BPP::FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour)
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
		uint8_t* VRAMptr = lines[y];
		VRAMptr += (x >> 2);

		uint8_t data = *VRAMptr;
		uint8_t mask = bitmaskTable[x & 3];
		int count = width;

		while (count--)
		{
			data = (data & (~mask)) | (colour & mask);
			mask >>= 2;
			if (!mask)
			{
				*VRAMptr++ = data;
				while (count > 4)
				{
					*VRAMptr++ = colour;
					count -= 4;
				}
				mask = 0xc0;
				data = *VRAMptr;
			}
		}

		*VRAMptr = data;

		height--;
		y++;
	}
}

void DrawSurface_2BPP::DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style)
{
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	int startX = x;
	uint8_t glyphHeight = font->glyphHeight;

	if (x >= context.clipRight)
	{
		return;
	}
	if (y >= context.clipBottom)
	{
		return;
	}
	if (y + glyphHeight > context.clipBottom)
	{
		glyphHeight = (uint8_t)(context.clipBottom - y);
	}
	if (y + glyphHeight < context.clipTop)
	{
		return;
	}

	uint8_t firstLine = 0;
	if (y < context.clipTop)
	{
		firstLine += context.clipTop - y;
		y += firstLine;
	}

	while (*text)
	{
		unsigned char c = (unsigned char) *text++;

		if (c < 32)
		{
			continue;
		}

		int index = c - 32;
		uint8_t glyphWidth = font->glyphWidth[index];

		if (glyphWidth == 0)
		{
			continue;
		}

		if (x + glyphWidth > context.clipRight)
		{
			break;
		}

		uint8_t* glyphData = font->glyphData + (font->glyphDataStride * index);

		glyphData += (firstLine * font->glyphWidthBytes);

		int outY = y;

		for (uint8_t j = firstLine; j < glyphHeight; j++)
		{
			uint8_t* VRAMptr = lines[outY] + (x >> 2);
			uint8_t writeData = *VRAMptr;
			uint8_t writeMask = bitmaskTable[x & 3];
			uint8_t writeOffset = (uint8_t)(x) & 0x7;

			if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
			{
				writeOffset++;
			}

			for (uint8_t i = 0; i < font->glyphWidthBytes; i++)
			{
				uint8_t glyphPixels = *glyphData++;

				for (uint8_t k = 0; k < 8; k++)
				{
					if (glyphPixels & (0x80 >> k))
					{
						writeData = (writeData & (~writeMask)) | (writeMask & colour);
					}

					writeMask >>= 2;
					if (!writeMask)
					{
						*VRAMptr++ = writeData;
						writeData = *VRAMptr;
						writeMask = 0xc0;
					}
				}
			}

			*VRAMptr = writeData;

			outY++;
		}

		x += glyphWidth;

		//if (x >= context.clipRight)
		//{
		//	break;
		//}
	}

	if ((style & FontStyle::Underline) && y - firstLine + font->glyphHeight - 1 < context.clipBottom)
	{
		HLine(context, startX, y - firstLine + font->glyphHeight - 1 - context.drawOffsetY, x - startX - context.drawOffsetX, colour);
	}
}

void DrawSurface_2BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
{
	if (!image->lines)
		return;

	x += context.drawOffsetX;
	y += context.drawOffsetY;

	int srcWidth = image->width;
	int srcHeight = image->height;
	int startX = 0;
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
		for (int j = 0; j < destHeight; j++)
		{
			uint8_t* src = image->lines[j + srcY].Get<uint8_t>() + srcX;
			uint8_t* dest = lines[y + j] + (x >> 2);
			uint8_t destMask = bitmaskTable[x & 3];
			uint8_t destBuffer = *dest;

			for (int i = 0; i < destWidth; i++)
			{
				uint8_t srcBuffer = *src;
//				srcBuffer |= (srcBuffer << 4);
				destBuffer = (destBuffer & (~destMask)) | ((srcBuffer) & destMask);
				src++;
				destMask >>= 2;
				if (!destMask)
				{
					*dest++ = destBuffer;
					destBuffer = *dest;
					destMask = 0xc0;
				}
			}
			*dest = destBuffer;
		}
	}
}


void DrawSurface_2BPP::InvertRect(DrawContext& context, int x, int y, int width, int height)
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
		uint8_t* VRAMptr = lines[y];
		VRAMptr += (x >> 2);
		int count = width;
		uint8_t data = *VRAMptr;
		uint8_t mask = (0xc0 >> (x & 3));

		while (count--)
		{
			data ^= mask;
			mask >>= 2;
			if (!mask)
			{
				*VRAMptr++ = data;
				while (count > 4)
				{
					*VRAMptr++ ^= 0xff;
					count -= 4;
				}
				mask = 0xc0;
				data = *VRAMptr;
			}
		}

		*VRAMptr = data;

		height--;
		y++;
	}
}

void DrawSurface_2BPP::VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size)
{
#if 0
	x += context.drawOffsetX;
	y += context.drawOffsetY;
	int startY = y;

	x >>= 3;
	const int grabSize = 7;
	const int minWidgetSize = grabSize + 4;
	const int widgetPaddingSize = size - minWidgetSize;
	int topPaddingSize = widgetPaddingSize >> 1;
	int bottomPaddingSize = widgetPaddingSize - topPaddingSize;
	const uint16_t edge = 0;
	const uint16_t inner = 0xfe7f;
	int bottomSpacing = height - position - size;

	while (position--)
	{
		*(uint16_t*)(&lines[y++][x]) = inner;
	}

	const uint16_t widgetEdge = 0x0660;
	*(uint16_t*)(&lines[y++][x]) = inner;
	*(uint16_t*)(&lines[y++][x]) = widgetEdge;

	const uint16_t widgetInner = 0xfa5f;
	const uint16_t grab = 0x0a50;

	while (topPaddingSize--)
	{
		*(uint16_t*)(&lines[y++][x]) = widgetInner;
	}

	*(uint16_t*)(&lines[y++][x]) = widgetInner;
	*(uint16_t*)(&lines[y++][x]) = grab;
	*(uint16_t*)(&lines[y++][x]) = widgetInner;
	*(uint16_t*)(&lines[y++][x]) = grab;
	*(uint16_t*)(&lines[y++][x]) = widgetInner;
	*(uint16_t*)(&lines[y++][x]) = grab;
	*(uint16_t*)(&lines[y++][x]) = widgetInner;

	while (bottomPaddingSize--)
	{
		*(uint16_t*)(&lines[y++][x]) = widgetInner;
	}

	*(uint16_t*)(&lines[y++][x]) = widgetEdge;
	*(uint16_t*)(&lines[y++][x]) = inner;

	while (bottomSpacing--)
	{
		*(uint16_t*)(&lines[y++][x]) = inner;
	}
#endif
}

void DrawSurface_2BPP::Clear()
{
	int widthBytes = width >> 2;
	for (int y = 0; y < height; y++)
	{
		memset(lines[y], 0xff, widthBytes);
	}
}
