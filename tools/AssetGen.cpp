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
#include <vector>

#include "../src/DataPack.h"

using namespace std;

//void EncodeImage(const char* imageFilename, ofstream& outputFile, const char* varName);
//void EncodeFont(const char* imageFilename, ofstream& outputFile, const char* varName);
//void EncodeCursor(const char* imageFilename, ofstream& outputFile, const char* varName, int hotSpotX, int hotSpotY);
//void GenerateDummyFont(ofstream& outputFile, const char* varName);

void EncodeFont(const char* basePath, const char* name, vector<uint8_t>& output, bool generateBold = true);
void EncodeCursor(const char* basePath, const char* name, vector<uint8_t>& output);
void EncodeImage(const char* basePath, const char* name, vector<uint8_t>& output);

void GeneratePaletteLUTs(const char* filename);


void AddEntryHeader(const char* name, vector<DataPackEntry>& entries, vector<uint8_t>& data)
{
	DataPackEntry entry;
	memset(entry.name, 0, 8);
	strncpy_s(entry.name, name, 8);
	entry.offset = (uint32_t)(data.size());
	entries.push_back(entry);
}

void GenerateAssetPack(const char* name)
{
	char basePath[256];
	vector<uint8_t> data;
	vector<DataPackEntry> entries;

	snprintf(basePath, 256, "assets/%s/", name);

	AddEntryHeader("FHELV1", entries, data);
	EncodeFont(basePath, "Helv1.png", data, false);
	AddEntryHeader("FHELV2", entries, data);
	EncodeFont(basePath, "Helv2.png", data, false);
	AddEntryHeader("FHELV3", entries, data);
	EncodeFont(basePath, "Helv3.png", data, false);

	AddEntryHeader("FHELV1B", entries, data);
	EncodeFont(basePath, "Helv1.png", data, true);
	AddEntryHeader("FHELV2B", entries, data);
	EncodeFont(basePath, "Helv2.png", data, true);
	AddEntryHeader("FHELV3B", entries, data);
	EncodeFont(basePath, "Helv3.png", data, true);

	AddEntryHeader("FCOUR1", entries, data);
	EncodeFont(basePath, "Cour1.png", data, false);
	AddEntryHeader("FCOUR2", entries, data);
	EncodeFont(basePath, "Cour2.png", data, false);
	AddEntryHeader("FCOUR3", entries, data);
	EncodeFont(basePath, "Cour3.png", data, false);

	AddEntryHeader("FCOUR1B", entries, data);
	EncodeFont(basePath, "Cour1.png", data, true);
	AddEntryHeader("FCOUR2B", entries, data);
	EncodeFont(basePath, "Cour2.png", data, true);
	AddEntryHeader("FCOUR3B", entries, data);
	EncodeFont(basePath, "Cour3.png", data, true);

	AddEntryHeader("CMOUSE", entries, data);
	EncodeCursor(basePath, "mouse.png", data);
	AddEntryHeader("CLINK", entries, data);
	EncodeCursor(basePath, "mouse-link.png", data);
	AddEntryHeader("CTEXT", entries, data);
	EncodeCursor(basePath, "mouse-select.png", data);

	AddEntryHeader("IIMG", entries, data);
	EncodeImage(basePath, "image-icon.png", data);

	AddEntryHeader("END", entries, data);

	char outputPath[256];
	snprintf(outputPath, 256, "%s.dat", name);
	FILE* fs;
	
	fopen_s(&fs, outputPath, "wb");
	if (fs)
	{
		uint16_t numEntries = (uint16_t)(entries.size());
		for (int n = 0; n < entries.size(); n++)
		{
			entries[n].offset += sizeof(uint16_t) + sizeof(DataPackEntry) * numEntries;
		}

		fwrite(&numEntries, sizeof(uint16_t), 1, fs);
		fwrite(entries.data(), sizeof(DataPackEntry), numEntries, fs);

		fwrite(data.data(), 1, data.size(), fs);

		fclose(fs);
	}
}

void GenerateAssetPacks()
{
	GenerateAssetPack("CGA");
	GenerateAssetPack("EGA");
	GenerateAssetPack("LowRes");
	GenerateAssetPack("Default");
}

int main(int argc, char* argv)
{
	GenerateAssetPacks();

	GeneratePaletteLUTs("src/Palettes.inc");
#if 0
	// CGA resources
	{
		ofstream outputFile("src/DOS/CGAData.inc");

		outputFile << "// CGA resources" << endl;
		outputFile << "// This file is auto generated" << endl << endl;
		outputFile << "// Fonts:" << endl;

		EncodeFont("assets/CGA/Helv2.png", outputFile, "CGA_RegularFont");
		EncodeFont("assets/CGA/Helv1.png", outputFile, "CGA_SmallFont");
		EncodeFont("assets/CGA/Helv3.png", outputFile, "CGA_LargeFont");
		
		EncodeFont("assets/CGA/Cour2.png", outputFile, "CGA_RegularFont_Monospace");
		EncodeFont("assets/CGA/Cour1.png", outputFile, "CGA_SmallFont_Monospace");
		EncodeFont("assets/CGA/Cour3.png", outputFile, "CGA_LargeFont_Monospace");

		//EncodeFont("assets/LowRes/Helv2.png", outputFile, "CGA_RegularFont");
		//EncodeFont("assets/LowRes/Helv1.png", outputFile, "CGA_SmallFont");
		//EncodeFont("assets/LowRes/Helv3.png", outputFile, "CGA_LargeFont");
		//
		//EncodeFont("assets/LowRes/Cour2.png", outputFile, "CGA_RegularFont_Monospace");
		//EncodeFont("assets/LowRes/Cour1.png", outputFile, "CGA_SmallFont_Monospace");
		//EncodeFont("assets/LowRes/Cour3.png", outputFile, "CGA_LargeFont_Monospace");

		outputFile << endl;
		outputFile << "// Mouse cursors:" << endl;

		EncodeCursor("assets/CGA/mouse.png", outputFile, "CGA_MouseCursor", 0, 0);
		EncodeCursor("assets/CGA/mouse-link.png", outputFile, "CGA_MouseCursorHand", 6, 1);
		EncodeCursor("assets/CGA/mouse-select.png", outputFile, "CGA_MouseCursorTextSelect", 8, 6);

		outputFile << endl;
		outputFile << "// Images:" << endl;

		EncodeImage("assets/CGA/image-icon.png", outputFile, "CGA_ImageIcon");
		EncodeImage("assets/CGA/bullet.png", outputFile, "CGA_Bullet");

		outputFile.close();
	}

	// Default resources
	{
		ofstream outputFile("src/DOS/DefData.inc");

		outputFile << "// Default resources" << endl;
		outputFile << "// This file is auto generated" << endl << endl;
		outputFile << "// Fonts:" << endl;

		EncodeFont("assets/Default/Helv2.png", outputFile, "Default_RegularFont");
		EncodeFont("assets/Default/Helv1.png", outputFile, "Default_SmallFont");
		EncodeFont("assets/Default/Helv3.png", outputFile, "Default_LargeFont");

		EncodeFont("assets/Default/Cour2.png", outputFile, "Default_RegularFont_Monospace");
		EncodeFont("assets/Default/Cour1.png", outputFile, "Default_SmallFont_Monospace");
		EncodeFont("assets/Default/Cour3.png", outputFile, "Default_LargeFont_Monospace");

		outputFile << endl;
		outputFile << "// Mouse cursors:" << endl;

		EncodeCursor("assets/Default/mouse.png", outputFile, "Default_MouseCursor", 0, 0);
		EncodeCursor("assets/Default/mouse-link.png", outputFile, "Default_MouseCursorHand", 6, 1);
		EncodeCursor("assets/Default/mouse-select.png", outputFile, "Default_MouseCursorTextSelect", 8, 6);

		outputFile << endl;
		outputFile << "// Images:" << endl;

		EncodeImage("assets/Default/image-icon.png", outputFile, "Default_ImageIcon");
		EncodeImage("assets/Default/bullet.png", outputFile, "Default_Bullet");

		outputFile.close();
	}

	// Text mode resources
	{
		ofstream outputFile("src/DOS/TextData.inc");

		outputFile << "// Text mode resources" << endl;
		outputFile << "// This file is auto generated" << endl << endl;
		outputFile << "// Fonts:" << endl;

		GenerateDummyFont(outputFile, "TextMode_Font");
		outputFile << endl;
		outputFile.close();
	}
#endif
	return 0;
}
