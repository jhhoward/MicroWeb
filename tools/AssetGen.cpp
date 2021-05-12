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

		outputFile << endl;
		outputFile << "// Mouse cursors:" << endl;

		EncodeCursor("assets/CGA/mouse.png", outputFile, "CGA_MouseCursor", 0, 0);
		EncodeCursor("assets/CGA/mouse-link.png", outputFile, "CGA_MouseCursorHand", 6, 1);
		EncodeCursor("assets/CGA/mouse-select.png", outputFile, "CGA_MouseCursorTextSelect", 8, 6);

		outputFile.close();
	}

	return 0;
}
