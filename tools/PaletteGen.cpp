#include <windows.h>
#include <stdint.h>
#include <stdio.h>

const RGBQUAD egaPalette[] =
{
	{ 0x00, 0x00, 0x00 }, // Entry 0 - Black
	{ 0xAA, 0x00, 0x00 }, // Entry 1 - Blue
	{ 0x00, 0xAA, 0x00 }, // Entry 2 - Green
	{ 0xAA, 0xAA, 0x00 }, // Entry 3 - Cyan
	{ 0x00, 0x00, 0xAA }, // Entry 4 - Red
	{ 0xAA, 0x00, 0xAA }, // Entry 5 - Magenta
	{ 0x00, 0x55, 0xAA }, // Entry 6 - Brown
	{ 0xAA, 0xAA, 0xAA }, // Entry 7 - Light Gray
	{ 0x55, 0x55, 0x55 }, // Entry 8 - Dark Gray
	{ 0xFF, 0x55, 0x55 }, // Entry 9 - Light Blue
	{ 0x55, 0xFF, 0x55 }, // Entry 10 - Light Green
	{ 0xFF, 0xFF, 0x55 }, // Entry 11 - Light Cyan
	{ 0x55, 0x55, 0xFF }, // Entry 12 - Light Red
	{ 0xFF, 0x55, 0xFF }, // Entry 13 - Light Magenta
	{ 0x55, 0xFF, 0xFF }, // Entry 14 - Yellow
	{ 0xFF, 0xFF, 0xFF }, // Entry 15 - White
};

const RGBQUAD cgaPalette[] =
{
	{ 0x00, 0x00, 0x00 }, // Entry 0 - Black
	{ 0xFF, 0xFF, 0x55 }, // Entry 11 - Light Cyan
	{ 0x55, 0x55, 0xFF }, // Entry 12 - Light Red
	{ 0xFF, 0xFF, 0xFF }, // Entry 15 - White
};

const RGBQUAD cgaCompositePalette[] =
{
	{ 0x00, 0x00, 0x00 },
	{ 0x31, 0x6e, 0x00 },
	{ 0xff, 0x09, 0x31 },
	{ 0xff, 0x8a, 0x00 },
	{ 0x31, 0x00, 0xa7 },
	{ 0x76, 0x76, 0x76 },
	{ 0xff, 0x11, 0xec },
	{ 0xff, 0x92, 0xbb },
	{ 0x00, 0x5a, 0x31 },
	{ 0x00, 0xdb, 0x00 },
	{ 0x76, 0x76, 0x76 },
	{ 0xbb, 0xf7, 0x45 },
	{ 0x00, 0x63, 0xec },
	{ 0x00, 0xe4, 0xbb },
	{ 0xbb, 0x7f, 0xff },
	{ 0xff, 0xff, 0xff },
};

int GetClosestPaletteIndex(const RGBQUAD colour, const RGBQUAD* palette, int paletteSize)
{
	int closest = -1;
	int closestDistance = 0;

	for (int n = 0; n < paletteSize; n++)
	{
		int distance = 0;
		distance += (palette[n].rgbRed - colour.rgbRed) * (palette[n].rgbRed - colour.rgbRed);
		distance += (palette[n].rgbGreen - colour.rgbGreen) * (palette[n].rgbGreen - colour.rgbGreen);
		distance += (palette[n].rgbBlue - colour.rgbBlue) * (palette[n].rgbBlue - colour.rgbBlue);

		if (closest == -1 || distance < closestDistance)
		{
			closest = n;
			closestDistance = distance;
		}
	}

	return closest;
}

void GeneratePaletteLUT(FILE* fs, const char* name, const RGBQUAD* palette, int paletteSize, bool fillByte)
{
	RGBQUAD colour;
	int count = 0;

	fprintf(fs, "uint8_t %s[] = {\n\t", name);

	for (int n = 0; n < 256; n++)
	{
		int r = (n & 0xe0);
		int g = (n & 0x1c) << 3;
		int b = (n & 3) << 6;

		colour.rgbBlue = (b * 255) / 0xc0;
		colour.rgbGreen = (g * 255) / 0xe0;
		colour.rgbRed = (r * 255) / 0xe0;
				
		int index = GetClosestPaletteIndex(colour, palette, paletteSize);

		if (fillByte)
		{
			if (paletteSize == 4)
			{
				index |= (index << 2);
			}
			//index |= (index << 4);
		}

		fprintf(fs, "%d", index);
		if (count != 255)
		{
			fprintf(fs, ", ");
			if ((count % 16) == 15)
			{
				fprintf(fs, "\n\t");
			}
		}

		count++;
	}

	fprintf(fs, "\n};\n");
}

void GeneratePaletteLUTs(const char* filename)
{
	FILE* fs;
	
	if (!fopen_s(&fs, filename, "w"))
	{
		GeneratePaletteLUT(fs, "egaPaletteLUT", egaPalette, 16, false);
		GeneratePaletteLUT(fs, "cgaPaletteLUT", cgaPalette, 4, true);
		GeneratePaletteLUT(fs, "compositeCgaPaletteLUT", cgaCompositePalette, 16, true);

		fclose(fs);
	}
}
