#include <memory.h>
#include "Surf8bpp.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"
#include "../Colour.h"

DrawSurface_8BPP::DrawSurface_8BPP(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
	bpp = 8;
}

void DrawSurface_8BPP::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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
	VRAMptr += x;

	while (count--)
	{
		*VRAMptr++ = colour;
	}
}

void DrawSurface_8BPP::VLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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

	while (count--)
	{
		(lines[y])[x] = colour;
		y++;
	}
}

void DrawSurface_8BPP::FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour)
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
		VRAMptr += x;
		int count = width;

		while (count--)
		{
			*VRAMptr++ = colour;
		}

		height--;
		y++;
	}
}

void DrawSurface_8BPP::DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style)
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
		uint8_t* VRAMptr = lines[y] + x;

		for (uint8_t j = firstLine; j < glyphHeight; j++)
		{
			if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
			{
				VRAMptr++;
			}

			for (uint8_t i = 0; i < font->glyphWidthBytes; i++)
			{
				uint8_t glyphPixels = *glyphData++;

				for (uint8_t k = 0; k < 8; k++)
				{
					if (glyphPixels & (0x80 >> k))
					{
						*VRAMptr = colour;
					}
					VRAMptr++;
				}
			}

			outY++;
			VRAMptr = lines[outY] + x;
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

void DrawSurface_8BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
{
	if (!image->lines)
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
			uint8_t* src = image->lines[srcY + j].Get<uint8_t>() + srcX;
			uint8_t* destRow = lines[y + j] + x;

			for (int i = 0; i < destWidth; i++)
			{
				uint8_t pixel = *src++;

				if (pixel != TRANSPARENT_COLOUR_VALUE)
				{
					*destRow = pixel;
				}
				destRow++;
			}
		}
	}
	else if (image->bpp == 1)
	{
		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			uint8_t* src = image->lines[srcY + j].Get<uint8_t>() + (srcX >> 3);
			uint8_t srcMask = 0x80 >> (srcX & 7);
			uint8_t* destRow = lines[y + j] + x;
			uint8_t black = 0;
			uint8_t white = 0xf;
			uint8_t buffer = *src++;

			for (int i = 0; i < destWidth; i++)
			{
				if (buffer & srcMask)
				{
					*destRow++ = white;
				}
				else
				{
					*destRow++ = black;
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

#if 0
	x += context.drawOffsetX;
	y += context.drawOffsetY;

	int srcWidth = image->width;
	int srcHeight = image->height;
	int srcPitch = image->pitch;
	uint8_t* srcData = image->data;

	// Calculate the destination width and height to copy, considering clipping region
	int destWidth = srcWidth;
	int destHeight = srcHeight;

	if (x < context.clipLeft)
	{
		srcData += ((context.clipLeft - x) >> 3);
		destWidth -= (context.clipLeft - x);
		x = context.clipLeft;
	}

	if (x + destWidth > context.clipRight)
	{
		destWidth = context.clipRight - x;
	}

	if (y < context.clipTop)
	{
		srcData += (context.clipTop - y) * srcPitch;
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

	int srcByteWidth = (srcWidth + 7) >> 3; // Calculate the width of the source image in bytes
	int destByteWidth = (destWidth + 7) >> 3; // Calculate the width of the destination image in bytes

	// Blit the image data line by line
	for (int j = 0; j < destHeight; j++)
	{
		uint8_t* srcRow = srcData + (j * srcPitch);
		uint8_t* destRow = lines[y + j] + (x >> 3);
		int xBits = 7 - (x & 0x7); // Number of bits in the first destination byte to skip

		// Handle the first destination byte separately with proper masking
		if (xBits == 0)
		{
			// If x is on a byte boundary, copy the whole byte from the source
			for (int i = 0; i < destByteWidth; i++)
			{
				destRow[i] = srcRow[i];
			}
		}
		else
		{
			// Copy the first destination byte with proper masking
			destRow[0] = (destRow[0] & (0xFF << xBits)) | (srcRow[0] >> (8 - xBits));

			// Copy the remaining bytes
			for (int i = 1; i < destByteWidth; i++)
			{
				destRow[i] = (srcRow[i - 1] << xBits) | (srcRow[i] >> (8 - xBits));
			}
		}

		// Handle the case when the destination image width is not a multiple of 8 bits
		int lastBit = 7 - ((x + destWidth) & 0x7);
		if (lastBit > 0)
		{
			int mask = 0xFF << lastBit;
			destRow[destByteWidth - 1] = (destRow[destByteWidth - 1] & ~mask) | (srcRow[destByteWidth] & mask);
		}
	}
#endif
}


void DrawSurface_8BPP::InvertRect(DrawContext& context, int x, int y, int width, int height)
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
		VRAMptr += x;
		int count = width;

		while (count--)
		{
			*VRAMptr++ ^= 0xf;
		}

		height--;
		y++;
	}
}

void DrawSurface_8BPP::VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size)
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

void DrawSurface_8BPP::Clear()
{
	int widthBytes = width;
	for (int y = 0; y < height; y++)
	{
		memset(lines[y], 0xf, widthBytes);
	}
}
