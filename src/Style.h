#ifndef _STYLE_H_
#define _STYLE_H_

#include <stdint.h>
#include "Font.h"

#pragma pack(push, 1)

struct ElementAlignment
{
	enum Type
	{
		Left = 0,
		Center = 1,
		Right = 2
	};
};

struct StyleOverrideMask
{
	union
	{
		struct
		{
			FontStyle::Type fontStyle : 4;
			bool fontSize : 1;
			bool fontSizeDelta : 1;
			bool alignment : 1;
			bool fontColour : 1;
		};
		uint8_t mask;
	};

	void Clear()
	{
		mask = 0;
	}
};

struct ElementStyle
{
	FontStyle::Type fontStyle : 4;
	int fontSize : 4;
	ElementAlignment::Type alignment : 2;
	uint8_t fontColour;
};

struct ElementStyleOverride
{
	StyleOverrideMask overrideMask;		// Mask of which style settings are overriden
	ElementStyle styleSettings;			// The style settings

	ElementStyleOverride()
	{
		overrideMask.Clear();
	}

	inline void SetFontStyle(FontStyle::Type fontStyle)
	{
		overrideMask.fontStyle = fontStyle;
		styleSettings.fontStyle = fontStyle;
	}

	inline void SetFontSize(int fontSize)
	{
		overrideMask.fontSize = true;
		styleSettings.fontSize = fontSize;
	}

	inline void SetFontSizeDelta(int delta)
	{
		overrideMask.fontSizeDelta = true;
		styleSettings.fontSize = delta;
	}

	inline void SetAlignment(ElementAlignment::Type alignment)
	{
		overrideMask.alignment = true;
		styleSettings.alignment = alignment;
	}

	inline void SetFontColour(uint8_t colour)
	{
		overrideMask.fontColour = true;
		styleSettings.fontColour = colour;
	}

	inline void Apply(ElementStyle& style)
	{
		if (overrideMask.fontStyle)
		{
			style.fontStyle = (FontStyle::Type)((style.fontStyle & (~overrideMask.fontStyle)) | styleSettings.fontStyle);
		}
		if (overrideMask.alignment)
		{
			style.alignment = styleSettings.alignment;
		}
		if (overrideMask.fontSize)
		{
			style.fontSize = styleSettings.fontSize;
		}
		if (overrideMask.fontSizeDelta)
		{
			style.fontSize += styleSettings.fontSize;
		}
		if (overrideMask.fontColour)
		{
			style.fontColour = styleSettings.fontColour;
		}
	}
};

#pragma pack(pop)

#endif
