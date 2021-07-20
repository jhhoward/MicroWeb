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

void EncodeImage(const char* imageFilename, ofstream& outputFile, const char* varName)
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

	for (unsigned int y = 0; y < height; y++)
	{
		uint8_t mask = 0;
		int maskPos = 0;

		for (unsigned int x = 0; x < width; x++)
		{
			int index = 4 * (y * width + x);
			uint8_t r = data[index];
			uint8_t g = data[index + 1];
			uint8_t b = data[index + 2];
			uint8_t a = data[index + 3];

			if (r < 127)
			{
				mask |= (1 << (7 - maskPos));
			}
			maskPos++;

			if (maskPos >= 8)
			{
				maskPos = 0;
				output.push_back(mask);
				mask = 0;
			}
		}
		if (maskPos != 0)
		{
			output.push_back(mask);
		}
	}

	outputFile << "static unsigned char " << varName << "_Data[] = {" << endl;
	outputFile << "\t";

	for (int n = 0; n < output.size(); n++)
	{
		int mask = output[n];
		outputFile << "0x" << setfill('0') << setw(2) << hex << mask << ", ";
	}

	outputFile << dec;
	outputFile << endl;

	outputFile << "};" << endl;
	outputFile << endl;

	outputFile << "Image " << varName << " = {" << endl;
	outputFile << "\t// Dimensions" << endl;
	outputFile << "\t" << width << ", " << height << "," << endl;
	outputFile << "\t" << varName << "_Data" << endl;
	outputFile << "};" << endl << endl;
}

