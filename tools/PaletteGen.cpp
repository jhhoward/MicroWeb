#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

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

// Function to convert sRGB to linear RGB
double sRGBtoLinear(double value) {
	if (value <= 0.04045)
		return value / 12.92;
	else
		return pow((value + 0.055) / 1.055, 2.4);
}

// Function to convert linear RGB to XYZ
double linearRGBtoXYZ(double value) {
	if (value > 0.04045)
		return pow((value + 0.055) / 1.055, 2.4);
	else
		return value / 12.92;
}

// Function to convert RGB to XYZ
void RGBtoXYZ(double R, double G, double B, double& X, double& Y, double& Z) {
	R = sRGBtoLinear(R / 255.0);
	G = sRGBtoLinear(G / 255.0);
	B = sRGBtoLinear(B / 255.0);

	X = 0.4124564 * R + 0.3575761 * G + 0.1804375 * B;
	Y = 0.2126729 * R + 0.7151522 * G + 0.0721750 * B;
	Z = 0.0193339 * R + 0.1191920 * G + 0.9503041 * B;
}

// Function to convert XYZ to CIELAB
void XYZtoLAB(double X, double Y, double Z, double& L, double& a, double& b) {
	X /= 0.95047;
	Y /= 1.0;
	Z /= 1.08883;

	if (X > 0.008856)
		X = pow(X, 1.0 / 3.0);
	else
		X = 7.787 * X + 16.0 / 116.0;
	if (Y > 0.008856)
		Y = pow(Y, 1.0 / 3.0);
	else
		Y = 7.787 * Y + 16.0 / 116.0;
	if (Z > 0.008856)
		Z = pow(Z, 1.0 / 3.0);
	else
		Z = 7.787 * Z + 16.0 / 116.0;

	L = 116.0 * Y - 16.0;
	a = 500.0 * (X - Y);
	b = 200.0 * (Y - Z);
}

// Function to compute distance between two CIELAB colors
double labDistance(double L1, double a1, double b1, double L2, double a2, double b2) {
	double deltaL = L1 - L2;
	double deltaA = a1 - a2;
	double deltaB = b1 - b2;
	return sqrt(deltaL * deltaL + deltaA * deltaA + deltaB * deltaB);
}

// Function to compute distance between two RGB colors using CIELAB
double rgbDistance(int R1, int G1, int B1, int R2, int G2, int B2) {
	double X1, Y1, Z1, X2, Y2, Z2;
	RGBtoXYZ(R1, G1, B1, X1, Y1, Z1);
	RGBtoXYZ(R2, G2, B2, X2, Y2, Z2);

	double L1, a1, b1, L2, a2, b2;
	XYZtoLAB(X1, Y1, Z1, L1, a1, b1);
	XYZtoLAB(X2, Y2, Z2, L2, a2, b2);

	return labDistance(L1, a1, b1, L2, a2, b2);
}

int GetClosestPaletteIndex(const RGBQUAD colour, const RGBQUAD* palette, int paletteSize)
{
	int closest = -1;
	double closestDistance = 0;
	bool useLABDistance = paletteSize < 16;

	for (int n = 0; n < paletteSize; n++)
	{
		double distance = 0;
		
		if (useLABDistance)
		{
			// LAB colour distance
			distance = rgbDistance(colour.rgbRed, colour.rgbGreen, colour.rgbBlue, palette[n].rgbRed, palette[n].rgbGreen, palette[n].rgbBlue);
		}
		else
		{
			// RGB Euclidean distance
			distance += (palette[n].rgbRed - colour.rgbRed) * (palette[n].rgbRed - colour.rgbRed);
			distance += (palette[n].rgbGreen - colour.rgbGreen) * (palette[n].rgbGreen - colour.rgbGreen);
			distance += (palette[n].rgbBlue - colour.rgbBlue) * (palette[n].rgbBlue - colour.rgbBlue);
		}

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
