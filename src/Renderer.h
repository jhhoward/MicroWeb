#pragma once
#include "Style.h"

#define MAX_STYLE_STACK_SIZE 32

class HTMLRenderer
{
public:
	HTMLRenderer() : currentLineLength(0), styleStackSize(0) 
	{
		styleStack[0] = 0;
	}
	void DrawText(const char* str);
	void BreakLine(int margin = 0);
	void EnsureNewLine();
	void IncreaseLineHeight(int lineHeight) {}
	
	void PushStyle(StyleMask style);
	void PopStyle();

private:
	int currentLineLength;
	StyleMask styleStack[MAX_STYLE_STACK_SIZE];
	uint8_t styleStackSize;
};
