#include "Font.h"

int Font::CalculateWidth(const char* text, FontStyle::Type style)
{
	int result = 0;

	while (*text)
	{
		char c = *text++;
		if (c < 32 || c >= 128)
		{
			continue;
		}
		char index = c - 32;

		result += glyphWidth[index];

		if (style & FontStyle::Bold)
		{
			result++;
		}
	}

	return result;
}
