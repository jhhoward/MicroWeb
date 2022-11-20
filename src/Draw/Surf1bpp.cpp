#include "Surf1bpp.h"
#include "../Font.h"

DrawSurface_1BPP::DrawSurface_1BPP(int inWidth, int inHeight)
	: DrawSurface(inWidth, inHeight)
{
	lines = new uint8_t * [height];
}

void DrawSurface_1BPP::HLine(DrawContext& context, int x, int y, int count, uint8_t colour)
{
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
		count = context.clipRight - count - 1;
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
	if (x + width >= context.clipRight)
	{
		width = context.clipRight - 1 - x;
	}
	if (y + height >= context.clipBottom)
	{
		height = context.clipBottom - 1 - y;
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
		char c = *text++;
		if (c < 32 || c >= 128)
		{
			continue;
		}

		char index = c - 32;
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
		HLine(context, startX, y - firstLine + font->glyphHeight - 1, x - startX, colour);
	}

}

void DrawSurface_1BPP::BlitImage(DrawContext& context, Image* image, int x, int y)
{

}
