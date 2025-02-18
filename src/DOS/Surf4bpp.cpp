#include <i86.h>
#include <conio.h>
#include <memory.h>
#include "../Draw/Surf4bpp.h"
#include "../Font.h"
#include "../Image/Image.h"
#include "../Memory/MemBlock.h"
#include "../Colour.h"

#define USE_ASM_ROUTINES 1

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
	format = DrawSurface::Format_4BPP_EGA;
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

	volatile uint8_t latchRead;

	// Set write mode 2
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x2);

	uint8_t* VRAMptr = lines[y];
	VRAMptr += (x >> 3);

	// Start pixels
	uint8_t mask = 0;
	while (count--)
	{
		mask |= pixelBitmasks[x & 7];
		x++;
		if (!(x & 7))
			break;
	}

	// Set bitmask
	outp(GC_INDEX, GC_BITMASK);
	outp(GC_DATA, mask);

	latchRead = *VRAMptr;
	*VRAMptr++ = colour;

	if (!count)
		return;

	// Middle bytes
	outp(GC_DATA, 0xff);
	while (count >= 8)
	{
		count -= 8;
		latchRead = *VRAMptr;
		*VRAMptr++ = colour;
	}

	// End byte
	if (count > 0)
	{
		mask = pixelEndBitmasks[count];
		outp(GC_DATA, mask);
		latchRead = *VRAMptr;
		*VRAMptr++ = colour;
	}

	// Restore write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);
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

	// Set write mode 2
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x2);

	uint8_t mask = pixelBitmasks[x & 7];
	int index = x >> 3;

	// Set bitmask
	outp(GC_INDEX, GC_BITMASK);
	outp(GC_DATA, mask);

	while (count--)
	{
		volatile uint8_t latchRead = (lines[y])[index];
		(lines[y])[index] = colour;
		y++;
	}

	// Restore write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);
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

	// Set write mode 2
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x2);

	volatile uint8_t latchRead;

	while (height)
	{
		int count = width;
		uint8_t* VRAMptr = lines[y];
		VRAMptr += (x >> 3);

		// Start pixels
		uint8_t mask = 0;
		int workX = x;
		while (count--)
		{
			mask |= pixelBitmasks[workX & 7];
			workX++;
			if (!(workX & 7))
				break;
		}

		// Set bitmask
		outp(GC_INDEX, GC_BITMASK);
		outp(GC_DATA, mask);

		latchRead = *VRAMptr;
		*VRAMptr++ = colour;

		if (count)
		{
			// Middle bytes
			outp(GC_DATA, 0xff);
			while (count >= 8)
			{
				count -= 8;

				latchRead = *VRAMptr;
				*VRAMptr++ = colour;
			}

			// End byte
			if (count > 0)
			{
				mask = pixelEndBitmasks[count];
				outp(GC_DATA, mask);

				latchRead = *VRAMptr;
				*VRAMptr++ = colour;
			}
		}

		height--;
		y++;
	}

	// Restore write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);
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

	// Set write mode 2
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x2);

	// Set bit mask
	outp(GC_INDEX, GC_BITMASK);

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

		uint8_t* glyphData = font->glyphData + font->glyphs[index].offset;

		glyphData += (firstLine * glyphWidthBytes);

		int outY = y;
		uint8_t* VRAMptr = lines[y] + (x >> 3);

		if(x >= 0)
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

					volatile uint8_t latchRead;

					outp(GC_DATA, glyphPixels >> writeOffset);
					latchRead = VRAMptr[i];
					VRAMptr[i] = colour;

					outp(GC_DATA, glyphPixels << (8 - writeOffset));
					latchRead = VRAMptr[i + 1];
					VRAMptr[i + 1] = colour;
				}

				outY++;
				VRAMptr = lines[outY] + (x >> 3);
			}
		}

		x += glyphWidth;
	}

	if ((style & FontStyle::Underline) && y - firstLine + font->glyphHeight - 1 < context.clipBottom)
	{
		HLine(context, startX - context.drawOffsetX, y - firstLine + font->glyphHeight - 1 - context.drawOffsetY, x - startX, colour);
	}

	// Restore write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);
}

static void BlitLineASM(uint8_t* srcPtr, uint8_t* destLine, uint8_t destMask, int count);
#pragma aux BlitLineASM = \
	"push ds" \
	"mov ds, dx"       /* ds:si = src */ \ 
	"next_pixel:" \
	"lodsb" \
	"cmp al, 0xff" \
	"je pixel_done"    /* Skip if transparent (0xff) */ \
	"mov ah, al" \
	"mov dx, 0x3ce"    /* dx = GC_INDEX */ \
	"mov al, 8"        /* GC_BITMASK */ \
	"out dx, al" \
	"inc dx"           /* dx = GC_DATA */ \
	"mov al, bl" \
	"out dx, al"       /* Write bitmask register */ \
	"mov al, [es:di]"  /* Read latches */ \
	"mov [es:di], ah"  /* Write */ \
	"pixel_done:" \
	"ror bl, 1"        /* Rotate bitmask */ \
	"cmp bl, 0x80" \
	"jne rotated_mask" \
	"inc di"           /* Mask fully rotated, move to next byte */ \
	"rotated_mask:" \
	"dec cx" \
	"jnz next_pixel" \
	"pop ds" \
	modify [cx ax si di dx] \	
	parm[dx si][es di][bl][cx];

void DrawSurface_4BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
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
		// Set write mode 2
		outp(GC_INDEX, GC_MODE);
		outp(GC_DATA, 0x2);

		uint8_t startDestMask = 0x80 >> (x & 7);
		int destOffset = (x >> 3);

		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
			MemBlockHandle imageLine = imageLines[srcY + j];
			uint8_t* src = imageLine.Get<uint8_t*>() + srcX;

#if USE_ASM_ROUTINES
			uint8_t* dest = lines[y + j] + destOffset;
			BlitLineASM(src, dest, startDestMask, destWidth);
#else
			uint8_t* destRow = lines[y + j] + destOffset;
			uint8_t destMask = startDestMask;

			for (int i = 0; i < destWidth; i++)
			{
				uint8_t colour = *src++;

				if (colour != TRANSPARENT_COLOUR_VALUE)
				{
					// Set bitmask
					outp(GC_INDEX, GC_BITMASK);
					outp(GC_DATA, destMask);

					volatile uint8_t latchRead = *destRow;
					*destRow = colour;
				}

				destMask >>= 1;
				if (!destMask)
				{
					destMask = 0x80;
					destRow++;
				}
			}
#endif
		}

		// Restore write mode 0
		outp(GC_INDEX, GC_MODE);
		outp(GC_DATA, 0x0);
	}
	else
	{
		// Set write mode
		outp(GC_INDEX, GC_MODE);
		outp(GC_DATA, 0x0);

		outp(GC_INDEX, GC_ROTATE);
		outp(GC_DATA, 0);

		outp(GC_INDEX, GC_SET_RESET);
		outp(GC_DATA, 0x0);

		// Set bit mask
		outp(GC_INDEX, GC_BITMASK);

		// Blit the image data line by line
		for (int j = 0; j < destHeight; j++)
		{
			outp(GC_DATA, 0xff);

			MemBlockHandle* imageLines = image->lines.Get<MemBlockHandle*>();
			MemBlockHandle imageLine = imageLines[j + srcY];
			uint8_t* src = imageLine.Get<uint8_t*>() + (srcX >> 3);
			uint8_t* dest = lines[y + j] + (x >> 3);
			uint8_t srcMask = 0x80 >> (srcX & 7);
			uint8_t destMask = 0x80 >> (x & 7);
			uint8_t srcBuffer = *src++;
			uint8_t writeBitMask = 0;
			uint8_t destBuffer = *dest;

			outp(GC_DATA, writeBitMask);

			for (int i = 0; i < destWidth; i++)
			{
				writeBitMask |= destMask;
				if ((srcBuffer & srcMask))
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
					srcBuffer = *src++;
				}
				destMask >>= 1;
				if (!destMask)
				{
					outp(GC_DATA, writeBitMask);
					*dest++ = destBuffer;
					destBuffer = *dest;
					destMask = 0x80;
					writeBitMask = 0;
				}
			}

			if (writeBitMask)
			{
				outp(GC_DATA, writeBitMask);
				*dest++ = destBuffer;
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

	// Set write mode 2
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x2);

	outp(GC_INDEX, GC_ROTATE);
	outp(GC_DATA, GC_XOR);

	while (height)
	{
		int count = width;
		uint8_t* VRAMptr = lines[y];
		VRAMptr += (x >> 3);

		// Start pixels
		uint8_t mask = 0;
		int workX = x;
		while (count--)
		{
			mask |= pixelBitmasks[workX & 7];
			workX++;
			if (!(workX & 7))
				break;
		}

		// Set bitmask
		outp(GC_INDEX, GC_BITMASK);
		outp(GC_DATA, mask);

		*VRAMptr++ |= 0xff;

		if (count)
		{
			// Middle bytes
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

	// Restore write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);
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

	// Set write mode 0
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
	// Set write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);

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

	// Restore write mode 0
	outp(GC_INDEX, GC_MODE);
	outp(GC_DATA, 0x0);
}
