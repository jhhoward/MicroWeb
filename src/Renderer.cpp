#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "Renderer.h"

void HTMLRenderer::DrawText(const char* str)
{
	return;

	StyleMask style = styleStack[styleStackSize];

	while(*str)
	{
		int wordLength = 0;
		const char* wordPtr = str;
		while(*wordPtr && *wordPtr != ' ')
		{
			wordPtr++;
			wordLength++;
		}
		if(currentLineLength + wordLength >= 80)
		{
			putchar('\n');
			currentLineLength = 0;
		}
		for(int n = 0; n < wordLength; n++)
		{
			if(str[n] > 0)
			{
				if (style & Style_Bold)
				{
					putchar(toupper(str[n]));
				}
				else
				{
					putchar(str[n]);
				}
				currentLineLength++;
			}
		}
		
		str += wordLength;
		if(*str)
		{
			if(*str > 0)
			{
				if (style & Style_Bold)
				{
					putchar(toupper(*str));
				}
				else
				{
					putchar(*str);
				}
				currentLineLength++;
			}
			str++;
		}
	}
}

void HTMLRenderer::BreakLine(int margin)
{
	return;
	printf("\n");
	currentLineLength = 0;
}

void HTMLRenderer::EnsureNewLine()
{
	return;
	if(currentLineLength > 0)
	{
		printf("\n");
		currentLineLength = 0;
	}
}

void HTMLRenderer::PushStyle(StyleMask style)
{
	if (styleStackSize < MAX_STYLE_STACK_SIZE - 1)
	{
		styleStackSize++;
		styleStack[styleStackSize] = style;
	}
}

void HTMLRenderer::PopStyle()
{
	if (styleStackSize > 0)
	{
		styleStackSize--;
	}
}
