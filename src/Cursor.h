#pragma once
#include <stdint.h>

struct MouseCursor
{
	enum Type
	{
		Pointer,
		Hand,
		TextSelect
	};
};

struct MouseCursorData
{
	uint16_t data[32];
	int hotSpotX, hotSpotY;
};
