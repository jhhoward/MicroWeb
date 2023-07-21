#include <memory.h>
#include "Surf1bpp.h"
#include "../Font.h"
#include "../Image.h"

DrawSurface_1BPP::DrawSurface_1BPP(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
}

void DrawSurface_1BPP::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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
	VRAMptr += (x >> 3);

	uint8_t data = *VRAMptr;

	if (colour)
	{
		uint8_t mask = (0x80 >> (x & 7));

		while (count--)
		{
			data |= mask;
			x++;
			mask = (mask >> 1) | 0x80;
			if ((x & 7) == 0)
			{
				*VRAMptr++ = data;
				while (count > 8)
				{
					*VRAMptr++ = 0xff;
					count -= 8;
				}
				mask = 0x80;
				data = *VRAMptr;
			}
		}

		*VRAMptr = data;
	}
	else
	{
		uint8_t mask = ~(0x80 >> (x & 7));

		while (count--)
		{
			data &= mask;
			x++;
			mask = (mask >> 1) | 0x80;
			if ((x & 7) == 0)
			{
				*VRAMptr++ = data;
				while (count > 8)
				{
					*VRAMptr++ = 0;
					count -= 8;
				}
				mask = ~0x80;
				data = *VRAMptr;
			}
		}

		*VRAMptr = data;
	}
}

void DrawSurface_1BPP::VLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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

	uint8_t mask = (0x80 >> (x & 7));
	int index = x >> 3;

	if (colour)
	{
		while (count--)
		{
			(lines[y])[index] |= mask;
			y++;
		}
	}
	else
	{
		mask ^= 0xff;
		while (count--)
		{
			(lines[y])[index] &= mask;
			y++;
		}
	}
}

void DrawSurface_1BPP::FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour)
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
		VRAMptr += (x >> 3);
		int count = width;
		int workX = x;
		uint8_t data = *VRAMptr;

		if (colour)
		{
			uint8_t mask = (0x80 >> (x & 7));

			while (count--)
			{
				data |= mask;
				workX++;
				mask = (mask >> 1) | 0x80;
				if ((workX & 7) == 0)
				{
					*VRAMptr++ = data;
					while (count > 8)
					{
						*VRAMptr++ = 0xff;
						count -= 8;
					}
					mask = 0x80;
					data = *VRAMptr;
				}
			}

			*VRAMptr = data;
		}
		else
		{
			uint8_t mask = ~(0x80 >> (x & 7));

			while (count--)
			{
				data &= mask;
				workX++;
				mask = (mask >> 1) | 0x80;
				if ((workX & 7) == 0)
				{
					*VRAMptr++ = data;
					while (count > 8)
					{
						*VRAMptr++ = 0;
						count -= 8;
					}
					mask = ~0x80;
					data = *VRAMptr;
				}
			}

			*VRAMptr = data;
		}

		height--;
		y++;
	}
}

void DrawSurface_1BPP::DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style)
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

		uint8_t* glyphData = font->glyphData + (font->glyphDataStride * index);

		glyphData += (firstLine * font->glyphWidthBytes);

		int outY = y;
		uint8_t* VRAMptr = lines[y] + (x >> 3);

		if (!colour)
		{
			for (uint8_t j = firstLine; j < glyphHeight; j++)
			{
				uint8_t writeOffset = (uint8_t)(x) & 0x7;

				if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
				{
					writeOffset++;
				}

				for (uint8_t i = 0; i < font->glyphWidthBytes; i++)
				{
					uint8_t glyphPixels = *glyphData++;

					if (style & FontStyle::Bold)
					{
						glyphPixels |= (glyphPixels >> 1);
					}

					VRAMptr[i] &= ~(glyphPixels >> writeOffset);
					VRAMptr[i + 1] &= ~(glyphPixels << (8 - writeOffset));
				}

				outY++;
				VRAMptr = lines[outY] + (x >> 3);
			}
		}
		else
		{
			for (uint8_t j = firstLine; j < glyphHeight; j++)
			{
				uint8_t writeOffset = (uint8_t)(x) & 0x7;

				if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
				{
					writeOffset++;
				}

				for (uint8_t i = 0; i < font->glyphWidthBytes; i++)
				{
					uint8_t glyphPixels = *glyphData++;

					if (style & FontStyle::Bold)
					{
						glyphPixels |= (glyphPixels >> 1);
					}

					VRAMptr[i] |= (glyphPixels >> writeOffset);
					VRAMptr[i + 1] |= (glyphPixels << (8 - writeOffset));
				}

				outY++;
				VRAMptr = lines[outY] + (x >> 3);
			}
		}

		x += glyphWidth;
		if (style & FontStyle::Bold)
		{
			x++;
		}

		if (x >= context.clipRight)
		{
			break;
		}
	}

	if ((style & FontStyle::Underline) && y - firstLine + font->glyphHeight - 1 < context.clipBottom)
	{
		HLine(context, startX, y - firstLine + font->glyphHeight - 1 - context.drawOffsetY, x - startX - context.drawOffsetX, colour);
	}

}

void DrawSurface_1BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
{
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
}


void DrawSurface_1BPP::InvertRect(DrawContext& context, int x, int y, int width, int height)
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
		VRAMptr += (x >> 3);
		int count = width;
		int workX = x;
		uint8_t data = *VRAMptr;
		uint8_t mask = (0x80 >> (x & 7));

		while (count--)
		{
			data ^= mask;
			workX++;
			//mask = (mask >> 1) | 0x80;
			mask >>= 1;
			if ((workX & 7) == 0)
			{
				*VRAMptr++ = data;
				while (count > 8)
				{
					*VRAMptr++ ^= 0xff;
					count -= 8;
				}
				mask = 0x80;
				data = *VRAMptr;
			}
		}

		*VRAMptr = data;

		height--;
		y++;
	}
}

void DrawSurface_1BPP::VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size)
{
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
}

void DrawSurface_1BPP::Clear()
{
	int widthBytes = width >> 3;
	for (int y = 0; y < height; y++)
	{
		memset(lines[y], 0xff, widthBytes);
	}
}
