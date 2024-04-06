#include <memory.h>
#include "Surf1512.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"
#include "../Platform.h"
#include "../App.h"

static uint8_t planeBits[4] = { 1, 2, 4, 8 };

static void SetColourSelect(uint8_t mask);
#pragma aux SetColourSelect = \
	"mov dx, 0x3d9" \
	"out dx, al" \
	modify[dx] \	
	parm[al];

static void SetBorderColour(uint8_t colour);
#pragma aux SetBorderColour = \
	"mov dx, 0x3df" \
	"out dx, al" \
	modify[dx] \	
	parm[al];

static void SetPlaneWriteMask(uint8_t mask);
#pragma aux SetPlaneWriteMask = \
	"mov dx, 0x3dd" \
	"out dx, al" \
	modify [dx] \	
	parm[al];

static void SetPlaneRead(uint8_t plane);
#pragma aux SetPlaneRead = \
	"mov dx, 0x3de" \
	"out dx, al" \
	modify [dx] \	
	parm[al];

DrawSurface_4BPP_PC1512::DrawSurface_4BPP_PC1512(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
	format = DrawSurface::Format_4BPP_PC1512;
}

void DrawSurface_4BPP_PC1512::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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

	uint8_t* VRAM = lines[y];
	VRAM += (x >> 3);

	for (int plane = 0; plane < 4; plane++)
	{
		SetPlaneRead(plane);
		uint8_t writeMask = planeBits[plane];
		SetPlaneWriteMask(writeMask);
		uint8_t* VRAMptr = VRAM;
		uint8_t data = *VRAMptr;
		int xCount = count;

		if (colour & writeMask)
		{
			uint8_t mask = (0x80 >> (x & 7));

			while (xCount--)
			{
				data |= mask;
				mask >>= 1;
				if (!mask)
				{
					*VRAMptr++ = data;
					while (xCount > 8)
					{
						*VRAMptr++ = 0xff;
						xCount -= 8;
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

			while (xCount--)
			{
				data &= mask;
				mask = (mask >> 1) | 0x80;
				if (mask == 0xff)
				{
					*VRAMptr++ = data;
					while (xCount > 8)
					{
						*VRAMptr++ = 0;
						xCount -= 8;
					}
					mask = ~0x80;
					data = *VRAMptr;
				}
			}

			*VRAMptr = data;
		}
	}
}

void DrawSurface_4BPP_PC1512::VLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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

	for (int plane = 0; plane < 4; plane++)
	{
		SetPlaneRead(plane);
		uint8_t planeMask = planeBits[plane];
		SetPlaneWriteMask(planeMask);
		int yCount = count;
		int outY = y;

		if (colour & planeMask)
		{
			while (yCount--)
			{
				(lines[outY])[index] |= mask;
				outY++;
			}
		}
		else
		{
			uint8_t andMask = mask ^ 0xff;
			while (yCount--)
			{
				(lines[outY])[index] &= andMask;
				outY++;
			}
		}
	}
}

void DrawSurface_4BPP_PC1512::FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour)
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

	while (height--)
	{
		for (int plane = 0; plane < 4; plane++)
		{
			SetPlaneRead(plane);
			uint8_t planeMask = planeBits[plane];
			SetPlaneWriteMask(planeMask);

			int count = width;
			uint8_t* VRAMptr = lines[y];
			VRAMptr += (x >> 3);
			uint8_t data = *VRAMptr;

			if (colour & planeMask)
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
					mask = (mask >> 1) | 0x80;
					if (mask == 0xff)
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
		y++;
	}
}

void DrawSurface_4BPP_PC1512::DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style)
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
		uint8_t glyphWidthBytes = (glyphWidth + 7) >> 3;

		if (glyphWidth == 0)
		{
			continue;
		}

		glyphWidth += bold;

		if (x + glyphWidth > context.clipRight)
		{
			break;
		}

		uint8_t* glyphDataPtr = font->glyphData + font->glyphs[index].offset;
		glyphDataPtr += (firstLine * glyphWidthBytes);

		for (int plane = 0; plane < 4; plane++)
		{
			int outY = y;
			uint8_t* VRAMptr = lines[y] + (x >> 3);
			uint8_t* glyphData = glyphDataPtr;

			SetPlaneRead(plane);

			uint8_t writeMask = planeBits[plane];
			SetPlaneWriteMask(writeMask);

			if (!(writeMask & colour))
			{
				for (uint8_t j = firstLine; j < glyphHeight; j++)
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
				for (uint8_t j = firstLine; j < glyphHeight; j++)
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

void DrawSurface_4BPP_PC1512::BlitImage(DrawContext& context, Image* image, int x, int y)
{
	if (!image->lines.IsAllocated() )
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

	if (image->bpp == 1)
	{
		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
			MemBlockHandle imageLine = imageLines[j + srcY];

			for (int plane = 0; plane < 4; plane++)
			{
				SetPlaneRead(plane);
				SetPlaneWriteMask(planeBits[plane]);

				uint8_t* src = imageLine.Get<uint8_t*>() + (srcX >> 3);
				uint8_t* dest = lines[y + j] + (x >> 3);
				uint8_t srcMask = 0x80 >> (srcX & 7);
				uint8_t destMask = 0x80 >> (x & 7);
				uint8_t srcBuffer = (*src++);
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
						srcBuffer = (*src++);
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
	}
	else
	{
		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
			MemBlockHandle imageLine = imageLines[j + srcY];

			for (int plane = 0; plane < 4; plane++)
			{
				SetPlaneRead(plane);
				uint8_t planeMask = planeBits[plane];
				SetPlaneWriteMask(planeMask);

				uint8_t* src = imageLine.Get<uint8_t*>() + (srcX >> 3);
				uint8_t* dest = lines[y + j] + (x >> 3);
				uint8_t destMask = 0x80 >> (x & 7);
				uint8_t srcBuffer = (*src++);
				uint8_t destBuffer = *dest;

				int count = destWidth;

				while(count)
				{
					uint8_t colour = *src++;

					if (colour != TRANSPARENT_COLOUR_VALUE)
					{
						if (colour & planeMask)
						{
							destBuffer |= destMask;
						}
						else
						{
							destBuffer &= ~destMask;
						}
					}

					destMask >>= 1;
					if (!destMask)
					{
						*dest++ = destBuffer;
						destBuffer = *dest;

						while (count >= 8)
						{
							// Unrolled 8 pixels at a time
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x80;
								else
									destBuffer &= ~0x80;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x40;
								else
									destBuffer &= ~0x40;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x20;
								else
									destBuffer &= ~0x20;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x10;
								else
									destBuffer &= ~0x10;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x8;
								else
									destBuffer &= ~0x8;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x4;
								else
									destBuffer &= ~0x4;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x2;
								else
									destBuffer &= ~0x2;
							colour = *src++;
							if (colour != TRANSPARENT_COLOUR_VALUE)
								if (colour & planeMask)
									destBuffer |= 0x1;
								else
									destBuffer &= ~0x1;

							*dest++ = destBuffer;
							destBuffer = *dest;
							count -= 8;
						}

						destMask = 0x80;
					}

					count--;
				}
				*dest = destBuffer;
			}
		}
	}
}


void DrawSurface_4BPP_PC1512::InvertRect(DrawContext& context, int x, int y, int width, int height)
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

	while (height--)
	{
		for (int plane = 0; plane < 4; plane++)
		{
			SetPlaneRead(plane);
			uint8_t planeMask = planeBits[plane];
			SetPlaneWriteMask(planeMask);

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
		}
		y++;
	}
}

void DrawSurface_4BPP_PC1512::VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size)
{
	const uint16_t widgetEdge = 0x0660;
	const uint16_t widgetInner = 0xfa5f;
	const uint16_t grab = 0x0a50;
	const uint16_t edge = 0;
	const uint16_t inner = 0xfe7f;

	SetPlaneWriteMask(0xf);

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

void DrawSurface_4BPP_PC1512::Clear()
{
	int widthBytes = width >> 3;
	uint8_t clear = 0xff;

//	SetBorderColour(0xf);
	SetColourSelect(0xf);
	SetPlaneWriteMask(0xf);

	for (int y = 0; y < height; y++)
	{
		memset(lines[y], clear, widthBytes);
	}
}

void DrawSurface_4BPP_PC1512::ScrollScreen(int top, int bottom, int width, int amount)
{
	width >>= 3;

	if (amount > 0)
	{
		for (int y = top; y < bottom; y++)
		{
			for (int plane = 0; plane < 4; plane++)
			{
				SetPlaneRead(plane);
				SetPlaneWriteMask(planeBits[plane]);
				memcpy(lines[y], lines[y + amount], width);
			}
		}
	}
	else if (amount < 0)
	{
		for (int y = bottom - 1; y >= top; y--)
		{
			for (int plane = 0; plane < 4; plane++)
			{
				SetPlaneRead(plane);
				SetPlaneWriteMask(planeBits[plane]);
				memcpy(lines[y], lines[y + amount], width);
			}
		}
	}
}

