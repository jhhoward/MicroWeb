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

#include <iostream>
#include <fstream>

using namespace std;

void EncodeFont(const char* imageFilename, ofstream& outputFile, const char* varName);
void EncodeCursor(const char* imageFilename, ofstream& outputFile, const char* varName, int hotSpotX, int hotSpotY);

int main(int argc, char* argv)
{
	// CGA resources
	{
		ofstream outputFile("src/DOS/CGAData.inc");

		outputFile << "// CGA resources" << endl;
		outputFile << "// This file is auto generated" << endl << endl;
		outputFile << "// Fonts:" << endl;

		EncodeFont("assets/CGA/font.png", outputFile, "CGA_RegularFont");
		EncodeFont("assets/CGA/font-small.png", outputFile, "CGA_SmallFont");
		EncodeFont("assets/CGA/font-large.png", outputFile, "CGA_LargeFont");

		EncodeFont("assets/CGA/font-mono.png", outputFile, "CGA_RegularFont_Monospace");
		EncodeFont("assets/CGA/font-mono-small.png", outputFile, "CGA_SmallFont_Monospace");
		EncodeFont("assets/CGA/font-mono-large.png", outputFile, "CGA_LargeFont_Monospace");

		outputFile << endl;
		outputFile << "// Mouse cursors:" << endl;

		EncodeCursor("assets/CGA/mouse.png", outputFile, "CGA_MouseCursor", 0, 0);
		EncodeCursor("assets/CGA/mouse-link.png", outputFile, "CGA_MouseCursorHand", 6, 1);
		EncodeCursor("assets/CGA/mouse-select.png", outputFile, "CGA_MouseCursorTextSelect", 8, 6);

		outputFile.close();
	}

	return 0;
}
