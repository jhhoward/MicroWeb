//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <vector>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "lodepng.h"

using namespace std;

/*
struct FontMetaData
{
	uint8_t glyphWidth[256 - 32];
	uint8_t glyphWidthBytes;
	uint8_t glyphHeight;
	uint8_t glyphDataStride;
};
*/

void EncodeFontOld(const char* imageFilename, ofstream& outputFile, const char* varName)
{
	vector<unsigned char> data;
	unsigned width, height;
	unsigned error = lodepng::decode(data, width, height, imageFilename);
	
	if(error)
	{
		cerr << "Error loading " << imageFilename << endl;
		return;
	}
	
	vector<uint8_t> output;
	vector<uint8_t> glyphWidths;
	vector<uint8_t> glyphBuffer;
	vector<uint16_t> offsets;
	
	int charIndex = 0;
	unsigned backCol = 0xffffff;

	for(unsigned int x = 0; x < width; x++)
	{
		vector<uint8_t> columnBuffer;

		for(unsigned int y = 0; y < height; y++)
		{
			int index = (y * width + x) * 4;
			unsigned col = data[index] | (data[index + 1] << 8) | (data[index + 2] << 16);
			
			if(col == 0)
			{
				columnBuffer.push_back(1);
			}
			else
			{
				if(col != backCol)
				{
					// Must be new glyph
					if(glyphBuffer.size())
					{
						uint16_t offset = (uint16_t)(output.size());
						offsets.push_back(offset);
						glyphWidths.push_back((uint8_t) (glyphBuffer.size() / height));
						output.insert(output.end(), glyphBuffer.begin(), glyphBuffer.end());
						glyphBuffer.clear();
					}
					
					backCol = col;
				}
				columnBuffer.push_back(0);
			}
		}
		glyphBuffer.insert(glyphBuffer.end(), columnBuffer.begin(), columnBuffer.end());
	}

	// Append last glyph
	if(glyphBuffer.size())
	{
		uint16_t offset = (uint16_t)(output.size());
		offsets.push_back(offset);
		glyphWidths.push_back((uint8_t) (glyphBuffer.size() / height));
		output.insert(output.end(), glyphBuffer.begin(), glyphBuffer.end());
		glyphBuffer.clear();
	}

	/*
	cout << "Font line height: " << height << endl;
	cout << "Num glyphs: " << offsets.size() << endl;
	
	for(int n = 0; n < offsets.size(); n++)
	{
		char c = (char) n + 32;
		cout << c << " : offset " << offsets[n] << " width: " << (int) glyphWidths[n] << endl;
	}
	*/
	
	int requiredBytes = 0;
	
	for(int n = 0; n < glyphWidths.size(); n++)
	{
		int width = glyphWidths[n];
		int needed = width / 8;
		if(width % 8)
		{
			needed++;
		}
		if(needed > requiredBytes)
		{
			requiredBytes = needed;
		}
	}
	
	outputFile << "static unsigned char " << varName << "_Data[] = {" << endl;
	
	for(int n = 0; n < offsets.size(); n++)
	{
		char c = (char)(n + 32);
		outputFile << "\t// '" << c << "'" << endl;
		outputFile << "\t";
		
		int glyphWidth = glyphWidths[n];
		for(unsigned int y = 0; y < height; y++)
		{
			int x = 0;
			
			for(int b = 0; b < requiredBytes; b++)
			{
				int mask = 0;
				
				for(int z = 0; z < 8; z++)
				{
					if(x < glyphWidth)
					{
						int index = offsets[n] + (x * height + y);
						if(output[index])
						{
							mask |= (0x80 >> z);
						}
					}
					
					x++;
				}
				
				outputFile << "0x" << setfill('0') << setw(2) << hex << mask << ", ";
			}
		}
		outputFile << endl;
	}

	outputFile << dec;

	outputFile << "};" << endl;
	outputFile << endl;
	
	outputFile << "Font " << varName << " = {" << endl;
	outputFile << "\t// Glyph widths" << endl;
	outputFile << "\t{ ";
	
	for(int n = 0; n < glyphWidths.size(); n++)
	{
		int width = glyphWidths[n];
		outputFile << width;
		if(n != glyphWidths.size() - 1)
		{
			outputFile << ",";
		}
	}	
	outputFile << "}, " << endl;
	outputFile << "\t" << requiredBytes << ", \t// Byte width" << endl;
	outputFile << "\t" << height << ", \t// Glyph height" << endl;
	int stride = requiredBytes * height;
	outputFile << "\t" << stride << ", \t// Glyph stride" << endl;
	outputFile << "\t" << varName << "_Data" << endl;
	outputFile << "};" << endl << endl;
}

void EncodeFont(const char* imageFilename, ofstream& outputFile, const char* varName)
{
	vector<unsigned char> data;
	unsigned width, height;
	unsigned error = lodepng::decode(data, width, height, imageFilename);

	if (error)
	{
		cerr << "Error loading " << imageFilename << endl;
		return;
	}

	vector<uint8_t> output;
	vector<uint8_t> glyphWidths;
	vector<uint8_t> glyphBuffer;
	vector<uint16_t> offsets;

	unsigned glyphHeight = height - 1;
	int charIndex = 0;

	for (unsigned int x = 1; x < width; x++)
	{
		vector<uint8_t> columnBuffer;

		if (glyphWidths.size() >= (128 - 32))
		{
			break;
		}

		int checkIndex = x * 4;
		if (data[checkIndex] > 0)
		{
			// This is a break for the next glyph
			uint16_t offset = (uint16_t)(output.size());
			offsets.push_back(offset);
			glyphWidths.push_back((uint8_t)(glyphBuffer.size() / glyphHeight));
			output.insert(output.end(), glyphBuffer.begin(), glyphBuffer.end());
			glyphBuffer.clear();

			continue;
		}

		for (unsigned int y = 0; y < glyphHeight; y++)
		{
			int index = ((y + 1) * width + x) * 4;
			unsigned col = data[index] | (data[index + 1] << 8) | (data[index + 2] << 16);

			if (col > 0)
			{
				columnBuffer.push_back(1);
			}
			else
			{
				columnBuffer.push_back(0);
			}
		}
		glyphBuffer.insert(glyphBuffer.end(), columnBuffer.begin(), columnBuffer.end());
	}

	
	cout << "Font line height: " << glyphHeight << endl;
	cout << "Num glyphs: " << offsets.size() << endl;

	//for(int n = 0; n < offsets.size(); n++)
	//{
	//	char c = (char) n + 32;
	//	cout << c << " : offset " << offsets[n] << " width: " << (int) glyphWidths[n] << endl;
	//}
	

	int requiredBytes = 0;

	for (int n = 0; n < glyphWidths.size(); n++)
	{
		int width = glyphWidths[n];
		int needed = width / 8;
		if (width % 8)
		{
			needed++;
		}
		if (needed > requiredBytes)
		{
			requiredBytes = needed;
		}
	}

	outputFile << "static unsigned char " << varName << "_Data[] = {" << endl;

	for (int n = 0; n < offsets.size(); n++)
	{
		char c = (char)(n + 32);
		outputFile << "\t// '" << c << "'" << endl;
		outputFile << "\t";

		int glyphWidth = glyphWidths[n];
		for (unsigned int y = 0; y < glyphHeight; y++)
		{
			int x = 0;

			for (int b = 0; b < requiredBytes; b++)
			{
				int mask = 0;

				for (int z = 0; z < 8; z++)
				{
					if (x < glyphWidth)
					{
						int index = offsets[n] + (x * glyphHeight + y);
						if (output[index])
						{
							mask |= (0x80 >> z);
						}
					}

					x++;
				}

				outputFile << "0x" << setfill('0') << setw(2) << hex << mask << ", ";
			}
		}
		outputFile << endl;
	}

	outputFile << dec;

	outputFile << "};" << endl;
	outputFile << endl;

	outputFile << "Font " << varName << " = {" << endl;
	outputFile << "\t// Glyph widths" << endl;
	outputFile << "\t{ ";

	for (int n = 0; n < glyphWidths.size(); n++)
	{
		int width = glyphWidths[n];
		outputFile << width;
		if (n != glyphWidths.size() - 1)
		{
			outputFile << ",";
		}
	}
	outputFile << "}, " << endl;
	outputFile << "\t" << requiredBytes << ", \t// Byte width" << endl;
	outputFile << "\t" << glyphHeight << ", \t// Glyph height" << endl;
	int stride = requiredBytes * glyphHeight;
	outputFile << "\t" << stride << ", \t// Glyph stride" << endl;
	outputFile << "\t" << varName << "_Data" << endl;
	outputFile << "};" << endl << endl;
}


void GenerateDummyFont(ofstream& outputFile, const char* varName)
{
	outputFile << "Font " << varName << " = {" << endl;
	outputFile << "\t// Glyph widths" << endl;
	outputFile << "\t{ ";

	for (int n = 0; n < 96; n++)
	{
		int width = 1;
		outputFile << width;
		if (n != 95)
		{
			outputFile << ",";
		}
	}
	outputFile << "}, " << endl;
	outputFile << "\t" << 0 << ", \t// Byte width" << endl;
	outputFile << "\t" << 1 << ", \t// Glyph height" << endl;
	outputFile << "\t" << 0 << ", \t// Glyph stride" << endl;
	outputFile << "\t" << "NULL" << endl;
	outputFile << "};" << endl << endl;
}

void EncodeFont(const char* basePath, const char* imageFilename, vector<uint8_t>& outputStream, uint16_t& headerOffsetPosition)
{
	headerOffsetPosition = (uint16_t) outputStream.size();

	char imageFilePath[256];
	snprintf(imageFilePath, 256, "%s%s", basePath, imageFilename);
	vector<unsigned char> data;
	unsigned width, height;
	unsigned error = lodepng::decode(data, width, height, imageFilePath);

	if (error)
	{
		cerr << "Error loading " << imageFilePath << endl;
		return;
	}

	vector<uint8_t> output;
	vector<uint8_t> glyphWidths;
	vector<uint8_t> glyphBuffer;
	vector<uint16_t> offsets;

	unsigned glyphHeight = height - 1;
	int charIndex = 0;

	for (unsigned int x = 1; x < width; x++)
	{
		vector<uint8_t> columnBuffer;

		if (glyphWidths.size() >= (256 - 32))
		{
			break;
		}

		int checkIndex = x * 4;
		if (data[checkIndex] > 0)
		{
			// This is a break for the next glyph
			uint16_t offset = (uint16_t)(output.size());
			offsets.push_back(offset);
			glyphWidths.push_back((uint8_t)(glyphBuffer.size() / glyphHeight));
			output.insert(output.end(), glyphBuffer.begin(), glyphBuffer.end());
			glyphBuffer.clear();

			continue;
		}

		for (unsigned int y = 0; y < glyphHeight; y++)
		{
			int index = ((y + 1) * width + x) * 4;
			unsigned col = data[index] | (data[index + 1] << 8) | (data[index + 2] << 16);

			if (col > 0)
			{
				columnBuffer.push_back(1);
			}
			else
			{
				columnBuffer.push_back(0);
			}
		}
		glyphBuffer.insert(glyphBuffer.end(), columnBuffer.begin(), columnBuffer.end());
	}


	cout << "Font line height: " << glyphHeight << endl;
	cout << "Num glyphs: " << offsets.size() << endl;

	//for(int n = 0; n < offsets.size(); n++)
	//{
	//	char c = (char) n + 32;
	//	cout << c << " : offset " << offsets[n] << " width: " << (int) glyphWidths[n] << endl;
	//}


	int requiredBytes = 0;

	for (int n = 0; n < glyphWidths.size(); n++)
	{
		int width = glyphWidths[n];
		int needed = width / 8;
		if (width % 8)
		{
			needed++;
		}
		if (needed > requiredBytes)
		{
			requiredBytes = needed;
		}
	}

	// Output the metadata 
	// uint8_t glyphWidth[256 - 32];
	// uint8_t glyphWidthBytes;
	// uint8_t glyphHeight;
	// uint8_t glyphDataStride;

	const int numNeededGlyphs = 256 - 32;

	for (int n = 0; n < numNeededGlyphs; n++)
	{
		if (n < glyphWidths.size())
		{
			outputStream.push_back(glyphWidths[n]);
		}
		else
		{
			outputStream.push_back(0);
		}
	}

	outputStream.push_back(requiredBytes);
	outputStream.push_back(glyphHeight);

	int stride = requiredBytes * glyphHeight;
	outputStream.push_back((uint8_t) stride);

	// Output the glyph data
	for (int n = 0; n < offsets.size(); n++)
	{
		int glyphWidth = glyphWidths[n];
		for (unsigned int y = 0; y < glyphHeight; y++)
		{
			int x = 0;

			for (int b = 0; b < requiredBytes; b++)
			{
				int mask = 0;

				for (int z = 0; z < 8; z++)
				{
					if (x < glyphWidth)
					{
						int index = offsets[n] + (x * glyphHeight + y);
						if (output[index])
						{
							mask |= (0x80 >> z);
						}
					}

					x++;
				}

				outputStream.push_back((uint8_t)mask);
			}
		}
	}

}