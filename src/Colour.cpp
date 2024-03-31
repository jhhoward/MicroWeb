#include "Colour.h"
#include "Palettes.inc"

ColourScheme monochromeColourScheme =
{
	// Page
	1,
	// Text
	0,
	// Link
	0,
	// Button
	1
};

ColourScheme cgaColourScheme =
{
	// Page
	0xff,
	// Text
	0,
	// Link
	0x55,
	// Button
	0xff
};

ColourScheme compositeCgaColourScheme =
{
	// Page
	0xff,
	// Text
	0,
	// Link
	0x22,
	// Button
	0x55
};

ColourScheme egaColourScheme = 
{
	// Page
	15,
	// Text
	0,
	// Link
	1,
	// Button
	7
};

ColourScheme colourScheme666 =
{
	// Page
	RGB666(0xff, 0xff, 0xff),
	// Text
	RGB666(0, 0, 0),
	// Link
	RGB666(0, 0, 0xee),
	// Button
	RGB666(0xcc, 0xcc, 0xcc),
};
