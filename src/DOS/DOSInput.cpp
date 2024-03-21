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

#include <dos.h>
#include <stdio.h>
#include <i86.h>
#include <conio.h>
#include <memory.h>
#include <stdint.h>
#include "DOSInput.h"
#include "../Keycodes.h"
#include "../DataPack.h"

void DOSInputDriver::Init()
{
	union REGS inreg, outreg;
	inreg.x.ax = 0;
	int86(0x33, &inreg, &outreg);
	hasMouse = (outreg.x.ax == 0xffff);

	currentCursor = MouseCursor::Hand;
	SetMouseCursor(MouseCursor::Pointer);
	mouseHideCount = 1;

	ShowMouse();
}

void DOSInputDriver::Shutdown()
{
	HideMouse();
}

void DOSInputDriver::ShowMouse()
{
	if (!hasMouse)
		return;

	mouseHideCount--;
	if (mouseHideCount > 0)
		return;

	union REGS inreg, outreg;
	inreg.x.ax = 1;
	int86(0x33, &inreg, &outreg);
}

void DOSInputDriver::SetMousePosition(int x, int y)
{
	if (!hasMouse)
		return;
	union REGS inreg, outreg;
	inreg.x.ax = 4;
	inreg.x.cx = x;
	inreg.x.dx = y;
	int86(0x33, &inreg, &outreg);
}

bool DOSInputDriver::GetMouseButtonPress(int& x, int& y)
{
	if (!hasMouse)
		return false;

	union REGS inreg, outreg;
	inreg.x.ax = 5;
	inreg.x.bx = 0;
	int86(0x33, &inreg, &outreg);
	x = outreg.x.cx;
	y = outreg.x.dx;
	return (outreg.x.bx > 0);
}

bool DOSInputDriver::GetMouseButtonRelease(int& x, int& y)
{
	if (!hasMouse)
		return false;

	union REGS inreg, outreg;
	inreg.x.ax = 6;
	inreg.x.bx = 0;
	int86(0x33, &inreg, &outreg);
	x = outreg.x.cx;
	y = outreg.x.dx;
	return (outreg.x.bx > 0);
}


void DOSInputDriver::HideMouse()
{
	if (!hasMouse)
		return;

	mouseHideCount++;
	if (mouseHideCount > 1)
		return;

	union REGS inreg, outreg;
	inreg.x.ax = 2;
	int86(0x33, &inreg, &outreg);
	mouseVisible = false;
}

static void SetMouseCursorASM(unsigned short far* data, uint16_t hotSpotX, uint16_t hotSpotY);
#pragma aux SetMouseCursorASM = \
	"mov ax, 9" \
	"int 0x33" \
	modify [ax] \
	parm [es dx] [bx] [cx] 


void DOSInputDriver::SetMouseCursor(MouseCursor::Type type)
{
	if (!hasMouse)
		return;
	if (type == currentCursor)
	{
		return;
	}
	MouseCursorData* cursor = Assets.GetMouseCursorData(type);
	if (cursor)
	{
		SetMouseCursorASM(cursor->data, cursor->hotSpotX, cursor->hotSpotY);
	}
	else
	{
		union REGS inreg, outreg;
		inreg.x.ax = 0xa;
		inreg.x.bx = 0;
		inreg.x.cx = 0xff00;
		switch (type)
		{
		case MouseCursor::Pointer:
			inreg.x.dx = 0x00db;
			break;
		case MouseCursor::Hand:
			inreg.x.dx = 0x00b1;
			break;
		case MouseCursor::TextSelect:
			inreg.x.dx = 0x00b3;
			break;
		}
		int86(0x33, &inreg, &outreg);
	}

	currentCursor = type;
}

void DOSInputDriver::GetMouseStatus(int& buttons, int& x, int& y)
{
	if (!hasMouse)
	{
		buttons = x = y = 0;
		return;
	}
	union REGS inreg, outreg;
	inreg.x.ax = 3;
	int86(0x33, &inreg, &outreg);
	x = outreg.x.cx;
	y = outreg.x.dx;
	buttons = outreg.x.bx;

	if (Platform::video->screenWidth == 320)
	{
		x /= 2;
	}
}

InputButtonCode DOSInputDriver::GetKeyPress() 
{
	if (kbhit())
	{
		InputButtonCode keyPress = getch();
		if (keyPress == 0)
		{
			keyPress = getch() << 8;
		}
		return keyPress;
	}

	return 0;
}
