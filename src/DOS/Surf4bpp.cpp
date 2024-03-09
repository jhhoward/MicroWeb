#include <i86.h>
#include <conio.h>
#include <memory.h>
#include "../Draw/Surf4bpp.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"
#include "../Colour.h"

#define GC_INDEX 0x3ce
#define GC_DATA 0x3cf

#define GC_SET_RESET 0
#define GC_MODE 0x5
#define GC_ROTATE 3
#define GC_BITMASK 8

#define SC_INDEX 0x3C4
#define SC_MAPMASK 2

#define GC_XOR 0x18

static uint8_t pixelBitmasks[8] =
{
	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

static uint8_t pixelStartBitmasks[8] =
{
	0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01
};

static uint8_t pixelEndBitmasks[8] =
{
	0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe
};

inline void SetPenColour(uint8_t colour)
{
	uint8_t temp;

	outp(GC_INDEX, GC_SET_RESET);
	temp = inp(GC_DATA);
	temp &= 0xf0;
	temp |= colour;
	outp(GC_DATA, temp);
}

DrawSurface_4BPP::DrawSurface_4BPP(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
	bpp = 4;
}

void DrawSurface_4BPP::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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

	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x3);

	SetPenColour(colour);

	uint8_t* VRAMptr = lines[y];
	uint8_t* VRAMend = VRAMptr;
	VRAMptr += (x >> 3);
	VRAMend += ((x + count) >> 3);

	if (VRAMptr == VRAMend)
	{
		// Starts and ends within the same byte
		uint8_t mask = 0;
		while (count--)
		{
			mask |= pixelBitmasks[x & 7];
			x++;
		}

		// Set bitmask
		outp(GC_INDEX, GC_BITMASK);
		outp(GC_DATA, mask);

		*VRAMptr |= 0xff;
	}
	else
	{
		uint8_t mask;

		// Start byte
		mask = pixelStartBitmasks[x & 7];
		outp(GC_INDEX, GC_BITMASK);
		outp(GC_DATA, mask);
		*VRAMptr++ |= 0xff;

		// Middle bytes
		count -= 8 - (x & 7);
		outp(GC_DATA, 0xff);
		while (count >= 8)
		{
			count -= 8;
			*VRAMptr++ |= 0xff;
		}

		// End byte
		if (count > 0)
		{
			mask = pixelEndBitmasks[count];
			outp(GC_DATA, mask);
			*VRAMptr++ |= 0xff;
		}
	}

}

void DrawSurface_4BPP::VLine(DrawContext& context, int x, int y, int count, uint8_t colour)
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

	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x3);

	SetPenColour(colour);

	uint8_t mask = pixelBitmasks[x & 7];
	int index = x >> 3;

	// Set bitmask
	outp(GC_INDEX, GC_BITMASK);
	outp(GC_DATA, mask);

	while (count--)
	{
		(lines[y])[index] |= mask;
		y++;
	}
}

void DrawSurface_4BPP::FillRect(DrawContext& context, int x, int y, int width, int height, uint8_t colour)
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

	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x3);

	SetPenColour(colour);

	while (height)
	{
		int count = width;
		uint8_t* VRAMptr = lines[y];
		uint8_t* VRAMend = VRAMptr;
		VRAMptr += (x >> 3);
		VRAMend += ((x + count) >> 3);

		if (VRAMptr == VRAMend)
		{
			// Starts and ends within the same byte
			uint8_t mask = 0;
			while (count--)
			{
				mask |= pixelBitmasks[x & 7];
				x++;
			}

			// Set bitmask
			outp(GC_INDEX, GC_BITMASK);
			outp(GC_DATA, mask);

			*VRAMptr |= 0xff;
		}
		else
		{
			uint8_t mask;

			// Start byte
			mask = pixelStartBitmasks[x & 7];
			outp(GC_INDEX, GC_BITMASK);
			outp(GC_DATA, mask);
			*VRAMptr++ |= 0xff;

			// Middle bytes
			count -= 8 - (x & 7);
			outp(GC_DATA, 0xff);
			while (count >= 8)
			{
				count -= 8;
				*VRAMptr++ |= 0xff;
			}

			// End byte
			if (count > 0)
			{
				mask = pixelEndBitmasks[count];
				outp(GC_DATA, mask);
				*VRAMptr++ |= 0xff;
			}
		}

		height--;
		y++;
	}
}

void DrawSurface_4BPP::DrawString(DrawContext& context, Font* font, const char* text, int x, int y, uint8_t colour, FontStyle::Type style)
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

	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x3);

	SetPenColour(colour);

	// Set bit mask
	outp(GC_INDEX, GC_BITMASK);

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

					outp(GC_DATA, glyphPixels >> writeOffset);
					VRAMptr[i] |= 0xff;
					outp(GC_DATA, glyphPixels << (8 - writeOffset));
					VRAMptr[i + 1] |= 0xff;

					//VRAMptr[i] |= (glyphPixels >> writeOffset);
					//VRAMptr[i + 1] |= (glyphPixels << (8 - writeOffset));
				}

				outY++;
				VRAMptr = lines[outY] + (x >> 3);
			}
		}

		x += glyphWidth;

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

void DrawSurface_4BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
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
		if (image->bpp == 1)
		{
			srcX += ((context.clipLeft - x) >> 3);
		}
		else
		{
			srcX += (context.clipLeft - x);
		}
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
		// Set write mode 3
		outp(GC_INDEX, GC_MODE);
		outp(GC_DATA, 0x3);

		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			uint8_t* src = image->lines[srcY + j].Get<uint8_t>() + srcX;
			uint8_t* destRow = lines[y + j] + (x >> 3);
			uint8_t destMask = 0x80 >> (x & 7);

			for (int i = 0; i < destWidth; i++)
			{
				uint8_t colour = *src++;

				if (colour != TRANSPARENT_COLOUR_VALUE)
				{
					// Set bitmask
					outp(GC_INDEX, GC_BITMASK);
					outp(GC_DATA, destMask);

					outp(GC_INDEX, GC_SET_RESET);
					outp(GC_DATA, colour);

					*destRow |= 0xff;
				}

				destMask >>= 1;
				if (!destMask)
				{
					destMask = 0x80;
					destRow++;
				}
			}
		}
	}
	else
	{
		int srcByteWidth = (srcWidth + 7) >> 3; // Calculate the width of the source image in bytes
		int destByteWidth = (destWidth + 7) >> 3; // Calculate the width of the destination image in bytes

		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			uint8_t* src = image->lines[srcY + j].Get<uint8_t>() + srcX;
			uint8_t* destRow = lines[y + j] + (x >> 3);
			int xBits = 7 - (x & 0x7); // Number of bits in the first destination byte to skip

			// Handle the first destination byte separately with proper masking
			if (xBits == 0)
			{
				// If x is on a byte boundary, copy the whole byte from the source
				for (int i = 0; i < destByteWidth; i++)
				{
					destRow[i] = src[i];
				}
			}
			else
			{
				// Copy the first destination byte with proper masking
				destRow[0] = (destRow[0] & (0xFF << xBits)) | (src[0] >> (8 - xBits));

				// Copy the remaining bytes
				for (int i = 1; i < destByteWidth; i++)
				{
					destRow[i] = (src[i - 1] << xBits) | (src[i] >> (8 - xBits));
				}
			}

			// Handle the case when the destination image width is not a multiple of 8 bits
			int lastBit = 7 - ((x + destWidth) & 0x7);
			if (lastBit > 0)
			{
				int mask = 0xFF << lastBit;
				destRow[destByteWidth - 1] = (destRow[destByteWidth - 1] & ~mask) | (src[destByteWidth] & mask);
			}
		}
	}
}


void DrawSurface_4BPP::InvertRect(DrawContext& context, int x, int y, int width, int height)
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

	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x3);

	outp(GC_INDEX, GC_ROTATE);
	outp(GC_DATA, GC_XOR);

	SetPenColour(0xf);

	while (height)
	{
		int count = width;
		uint8_t* VRAMptr = lines[y];
		uint8_t* VRAMend = VRAMptr;
		VRAMptr += (x >> 3);
		VRAMend += ((x + count) >> 3);

		if (VRAMptr == VRAMend)
		{
			// Starts and ends within the same byte
			uint8_t mask = 0;
			while (count--)
			{
				mask |= pixelBitmasks[x & 7];
				x++;
			}

			// Set bitmask
			outp(GC_INDEX, GC_BITMASK);
			outp(GC_DATA, mask);

			*VRAMptr |= 0xff;
		}
		else
		{
			uint8_t mask;

			// Start byte
			mask = pixelStartBitmasks[x & 7];
			outp(GC_INDEX, GC_BITMASK);
			outp(GC_DATA, mask);
			*VRAMptr++ |= 0xff;

			// Middle bytes
			count -= 8 - (x & 7);
			outp(GC_DATA, 0xff);
			while (count >= 8)
			{
				count -= 8;
				*VRAMptr++ |= 0xff;
			}

			// End byte
			if (count > 0)
			{
				mask = pixelEndBitmasks[count];
				outp(GC_DATA, mask);
				*VRAMptr++ |= 0xff;
			}
		}

		height--;
		y++;
	}

	outp(GC_INDEX, GC_ROTATE);
	outp(GC_DATA, 0);

}

void DrawSurface_4BPP::VerticalScrollBar(DrawContext& context, int x, int y, int height, int position, int size)
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

	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);

	SetPenColour(0xff);

	// Set bit mask
	outp(GC_INDEX, GC_BITMASK);
	outp(GC_DATA, 0xff);

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

void DrawSurface_4BPP::Clear()
{
	// Set write mode 3
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x3);

	SetPenColour(0xf);

	// Set bit mask
	outp(GC_INDEX, GC_BITMASK);
	outp(GC_DATA, 0xff);

	int widthBytes = width >> 3;
	for (int y = 0; y < height; y++)
	{
		memset(lines[y], 0xff, widthBytes);
	}
}

// Implementation of memcpy that copies one byte at a time
// Needed when copying video memory since plane values get 
// stored in VGA latches. Using standard memcpy() will
// copy with words for speed but ends up corrupting the
// plane information
static void memcpy_bytes(void far* dest, void far* src, unsigned int count);
#pragma aux memcpy_bytes = \
	"push ds" \
	"mov ds, dx" \
	"rep movsb" \
	"pop dx" \
	modify [si di cx] \	
	parm[es di][dx si][cx];


void DrawSurface_4BPP::ScrollScreen(int top, int bottom, int width, int amount)
{
	width >>= 3;

	// Set write mode 1
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x1);

	outp(GC_INDEX, GC_ROTATE);
	outp(GC_DATA, 0);

	// Set bit mask
	outp(GC_INDEX, GC_BITMASK);
	outp(GC_DATA, 0xff);

	if (amount > 0)
	{
		for (int y = top; y < bottom; y++)
		{
			memcpy_bytes(lines[y], lines[y + amount], width);
		}
	}
	else if (amount < 0)
	{
		for (int y = bottom - 1; y >= top; y--)
		{
			memcpy_bytes(lines[y], lines[y + amount], width);
		}
	}
}
