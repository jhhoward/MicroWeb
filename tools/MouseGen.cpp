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

void EncodeCursor(const char* imageFilename, ofstream& outputFile, const char* varName, int hotSpotX, int hotSpotY)
{
	vector<unsigned char> data;
	unsigned width, height;
	unsigned error = lodepng::decode(data, width, height, imageFilename);
	
	if(error)
	{
		cerr << "Error loading " << imageFilename << endl;
		return;
	}
	
	if(width != 16 || height != 16)
	{
		cerr << "Cursor must be 16x16" << endl;
		return;
	}
	
	vector<uint8_t> maskData;
	vector<uint8_t> colourData;
	
	for(int y = 0; y < 16; y++)
	{
		for(int x = 0; x < 16; x++)
		{
			int index = (y * width + x) * 4;
			unsigned col = data[index] | (data[index + 1] << 8) | (data[index + 2] << 16);
			
			if(data[index + 3] == 0)
			{
				maskData.push_back(1);
				colourData.push_back(0);
			}
			else
			{
				maskData.push_back(0);
				colourData.push_back(col == 0 ? 0 : 1);
			}
			
		}
	}

	outputFile << hex;
	outputFile << "MouseCursorData " << varName << " = {" << endl << "\t";
	
	outputFile << "{" << endl << "\t\t";
	uint16_t output = 0;
	for(int n = 0; n < 256; n++)
	{
		if(maskData[n] != 0)
		{
			output |= (0x8000 >> (n % 16));
		}
		if((n % 16) == 15)
		{
			outputFile << "0x" << setfill('0') << setw(4) << hex << output << ",";
			output = 0;
		}
	}

	outputFile << endl << "\t\t";
	
	output = 0;
	for(int n = 0; n < 256; n++)
	{
		if(colourData[n] != 0)
		{
			output |= (0x8000 >> (n % 16));
		}
		if((n % 16) == 15)
		{
			outputFile << "0x" << setfill('0') << setw(4) << hex << output << ",";
			output = 0;
		}
	}

	outputFile << endl;
	outputFile << "\t}," << endl;
	outputFile << dec;
	outputFile << "\t// Hot spot" << endl;
	outputFile << "\t" << hotSpotX << ", " << hotSpotY << endl;
	outputFile << "};" << endl << endl;
}

