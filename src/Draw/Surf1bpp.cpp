#include <memory.h>
#include "Surf1bpp.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"
#include "../Platform.h"
#include "../App.h"

DrawSurface_1BPP::DrawSurface_1BPP(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
	format = DrawSurface::Format_1BPP;
	cursorBufferX = -1;
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

	if (App::config.invertScreen)
		colour = !colour;

	if (colour)
	{
		uint8_t mask = (0x80 >> (x & 7));

		while (count--)
		{
			data |= mask;
			x++;
			mask >>= 1;
			if (!mask)
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
	if (y + count > context.clipBottom)
	{
		count = context.clipBottom - y;
	}
	if (count <= 0)
	{
		return;
	}

	uint8_t mask = (0x80 >> (x & 7));
	int index = x >> 3;

	if (App::config.invertScreen)
		colour = !colour;

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

	if (App::config.invertScreen)
		colour = !colour;

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
				mask >>= 1;
				if (!mask)
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

	if (App::config.invertScreen)
		colour = !colour;

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

		uint8_t* VRAMptr = lines[outY] + (x >> 3);

		if (x < 0)
		{

		}
		else if (!colour)
		{
			for(uint8_t j = glyphTop; j <= glyphBottom; j++)
			{
				uint8_t writeOffset = (uint8_t)(x) & 0x7;

				if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
				{
					writeOffset++;
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

					VRAMptr[i] &= ~(glyphPixels >> writeOffset);
					VRAMptr[i + 1] &= ~(glyphPixels << (8 - writeOffset));
				}

				outY++;
				VRAMptr = lines[outY] + (x >> 3);
			}
		}
		else
		{
			for (uint8_t j = glyphTop; j <= glyphBottom; j++)
			{
				uint8_t writeOffset = (uint8_t)(x) & 0x7;

				if ((style & FontStyle::Italic) && j < (font->glyphHeight >> 1))
				{
					writeOffset++;
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

					VRAMptr[i] |= (glyphPixels >> writeOffset);
					VRAMptr[i + 1] |= (glyphPixels << (8 - writeOffset));
				}

				outY++;
				VRAMptr = lines[outY] + (x >> 3);
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

void DrawSurface_1BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
{
	if (!image->lines.IsAllocated() || image->bpp != 1)
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

	uint8_t invertMask = App::config.invertScreen ? 0xff : 0;

	// Blit the image data line by line
	for (int j = 0; j < destHeight; j++)
	{
		MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
		MemBlockHandle imageLine = imageLines[j + srcY];
		uint8_t* src = imageLine.Get<uint8_t*>() + (srcX >> 3);
		uint8_t* dest = lines[y + j] + (x >> 3);
		uint8_t srcMask = 0x80 >> (srcX & 7);
		uint8_t destMask = 0x80 >> (x & 7);
		uint8_t srcBuffer = (*src++) ^ invertMask;
		uint8_t destBuffer = *dest;

		for (int i = 0; i < destWidth; i++)
		{
			if (srcBuffer & srcMask)
			{
				destBuffer |= destMask;
			}
			else
			{
				destBuffer &= ~destMask;
			}
			srcMask >>= 1;
			if (!srcMask)
			{
				srcMask = 0x80;
				srcBuffer = (*src++) ^ invertMask;
			}
			destMask >>= 1;
			if (!destMask)
			{
				*dest++ = destBuffer;
				destBuffer = *dest;
				destMask = 0x80;
			}
		}
		*dest = destBuffer;
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
		uint8_t data = *VRAMptr;
		uint8_t mask = (0x80 >> (x & 7));

		while (count--)
		{
			data ^= mask;
			//mask = (mask >> 1) | 0x80;
			mask >>= 1;
			if (!mask)
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
	uint16_t inverseMask = App::config.invertScreen ? 0xffff : 0;
	const uint16_t widgetEdge = 0x0660 ^ inverseMask;
	const uint16_t widgetInner = 0xfa5f ^ inverseMask;
	const uint16_t grab = 0x0a50 ^ inverseMask;
	const uint16_t edge = 0 ^ inverseMask;
	const uint16_t inner = 0xfe7f ^ inverseMask;

	x += context.drawOffsetX;
	y += context.drawOffsetY;
	int startY = y;

	x >>= 3;
	const int grabSize = 7;
	const int minWidgetSize = grabSize + 4;
	const int widgetPaddingSize = size - minWidgetSize;
	int topPaddingSize = widgetPaddingSize >> 1;
	int bottomPaddingSize = widgetPaddingSize - topPaddingSize;
	int bottomSpacing = height - position - size;

	while (position--)
	{
		*(uint16_t*)(&lines[y++][x]) = inner;
	}

	*(uint16_t*)(&lines[y++][x]) = inner;
	*(uint16_t*)(&lines[y++][x]) = widgetEdge;


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
	uint8_t clear = App::config.invertScreen ? 0 : 0xff;

	for (int y = 0; y < height; y++)
	{
		memset(lines[y], clear, widthBytes);
	}
}

void DrawSurface_1BPP::ScrollScreen(int top, int bottom, int width, int amount)
{
	width >>= 3;

	if (amount > 0)
	{
		for (int y = top; y < bottom; y++)
		{
			memcpy(lines[y], lines[y + amount], width);
		}
	}
	else if (amount < 0)
	{
		for (int y = bottom - 1; y >= top; y--)
		{
			memcpy(lines[y], lines[y + amount], width);
		}
	}
}


void DrawSurface_1BPP::DrawCursor(struct MouseCursorData* cursor, int x, int y)
{
	HideCursor();

	x -= cursor->hotSpotX;
	y -= cursor->hotSpotY;

	// First save screen state to the cursor buffer
	{
		cursorBufferX = x;
		if (cursorBufferX < 0)
		{
			cursorBufferX = 0;
		}
		if (cursorBufferX > width - 24)
		{
			cursorBufferX = width - 24;
		}
		cursorBufferX >>= 3;

		cursorBufferY = y;
		if (cursorBufferY < 0)
		{
			cursorBufferY = 0;
		}
		if (cursorBufferY > height - 16)
		{
			cursorBufferY = height - 16;
		}

		uint8_t* bufferPtr = cursorBuffer;
		for (int j = 0; j < 16; j++)
		{
			uint8_t* ptr = lines[cursorBufferY + j] + cursorBufferX;

			*bufferPtr++ = *ptr++;
			*bufferPtr++ = *ptr++;
			*bufferPtr++ = *ptr++;
		}
	}

	int cursorHeight = 16;
	uint16_t* cursorData = cursor->data;

	int shiftRight = x & 7;
	int shiftLeft = 8 - shiftRight;
	int addressOffset;

	if (x < 0)
	{
		addressOffset = 0;
	}
	else
	{
		addressOffset = x >> 3;
	}

	while (cursorHeight--)
	{
		if (y >= height)
			break;
		if (y >= 0)
		{
			uint8_t* ptr = (lines[y] + addressOffset);
			uint8_t* cursorMask = (uint8_t*)cursorData;
			uint8_t* cursorColour = (uint8_t*)(cursorData + 16);

			if (shiftRight)
			{
				if (x >= 0)
				{
					*ptr &= (0xff << shiftLeft) | (cursorMask[1] >> shiftRight);
					*ptr |= (cursorColour[1] >> shiftRight);
					ptr++;
				}
				if (x < width - 8)
				{
					*ptr &= (0xff >> shiftRight) | (cursorMask[1] << shiftLeft);
					*ptr |= (cursorColour[1] << (8 - shiftRight));
					*ptr &= (0xff << shiftLeft) | (cursorMask[0] >> shiftRight);
					*ptr |= (cursorColour[0] >> shiftRight);
				}
				if (x < width - 16)
				{
					ptr++;
					*ptr &= (0xff >> shiftRight) | (cursorMask[0] << shiftLeft);
					*ptr |= (cursorColour[0] << (8 - shiftRight));
				}
			}
			else
			{
				if (x >= 0)
				{
					*ptr &= (cursorMask[1] >> shiftRight);
					*ptr |= (cursorColour[1] >> shiftRight);
				}

				if (x < width - 8)
				{
					ptr++;
					*ptr &= (cursorMask[0] >> shiftRight);
					*ptr |= (cursorColour[0] >> shiftRight);
				}
			}
		}

		cursorData++;
		y++;
	}
}

void DrawSurface_1BPP::HideCursor()
{
	if (cursorBufferX < 0)
	{
		// Already hidden
		return;
	}

	uint8_t* bufferPtr = cursorBuffer;
	for (int j = 0; j < 16; j++)
	{
		uint8_t* ptr = lines[cursorBufferY + j] + cursorBufferX;

		*ptr++ = *bufferPtr++;
		*ptr++ = *bufferPtr++;
		*ptr++ = *bufferPtr++;
	}

	cursorBufferX = -1;
}
