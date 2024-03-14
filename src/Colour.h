#ifndef _COLOUR_H_
#define _COLOUR_H_

#include <stdint.h>

#define RGB332(red, green, blue) (((red) & 0xe0) | (((green) & 0xe0) >> 3) | (((blue) & 0xc0) >> 6))
#define RGB_TO_GREY(red, green, blue) (((red) * 76 + (green) * 150 + (blue) * 30) >> 8)
#define RGB666(red, green, blue) ( ( ( (red) * 5) / 255) * 36) + ( ( ( (green) * 5) / 255) * 6) + ( ( ( (blue) * 5) / 255) ) + 16 

#define TRANSPARENT_COLOUR_VALUE 0xff

struct ColourScheme
{
	uint8_t pageColour;
	uint8_t textColour;
	uint8_t linkColour;
	uint8_t buttonColour;
};

struct NamedColour
{
	const char* name;
	uint8_t colour;
};

extern ColourScheme monochromeColourScheme;
extern ColourScheme monochromeInverseColourScheme;
extern ColourScheme egaColourScheme;
extern ColourScheme cgaColourScheme;
extern ColourScheme compositeCgaColourScheme;
extern ColourScheme colourScheme666;

extern uint8_t cgaPaletteLUT[];
extern uint8_t egaPaletteLUT[];
extern uint8_t compositeCgaPaletteLUT[];

#endif
