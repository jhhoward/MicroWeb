#ifndef _COLOUR_H_
#define _COLOUR_H_

#include <stdint.h>

#define RGB332(red, green, blue) (((red) & 0xe0) | (((green) & 0xe0) >> 3) | (((blue) & 0xc0) >> 6))
#define RGB_TO_GREY(red, green, blue) (((red) * 76 + (green) * 150 + (blue) * 30) >> 8)

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
extern ColourScheme egaColourScheme;

extern uint8_t cgaPaletteLUT[];

#endif
