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
#include "../Draw/Surface.h"
#include "../VidModes.h"

void DOSInputDriver::Init()
{
	union REGS inreg, outreg;
	inreg.x.ax = 0;
	int86(0x33, &inreg, &outreg);
	hasMouse = (outreg.x.ax == 0xffff);

	currentCursor = MouseCursor::Hand;
	SetMouseCursor(MouseCursor::Pointer);
	mouseHideCount = 1;

	useMouseDriverCursor = Platform::video->GetVideoModeInfo()->useMouseDriverCursor;
	queuedPressX = -1;
	queuedPressY = -1;

	// Set Horizontal Range
	int horizontalRange = Platform::video->screenWidth;
	if (horizontalRange == 320) horizontalRange = 640;			// Due some strangeness of how DOS mouse drivers work
	inreg.w.ax = 0x7;
	inreg.w.cx = 0;          // Minimum X
	inreg.w.dx = horizontalRange - 1;  // Maximum X 
	int86(0x33, &inreg, &outreg);

	// Set Vertical Range 
	inreg.w.ax = 0x8;
	inreg.w.cx = 0;          // Minimum Y
	inreg.w.dx = Platform::video->screenHeight - 1;  // Maximum Y 
	int86(0x33, &inreg, &outreg);

	// Set mouse mickey ratio
	//inreg.w.ax = 0xf;
	//inreg.w.cx = 8;
	//inreg.w.dx = 16;
	//int86(0x33, &inreg, &outreg);

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

	if (useMouseDriverCursor)
	{
		union REGS inreg, outreg;
		inreg.x.ax = 1;
		int86(0x33, &inreg, &outreg);
	}

	lastMouseX = -1;
	mouseVisible = true;
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

	if (queuedPressX != -1)
	{
		x = queuedPressX;
		y = queuedPressY;
		queuedPressX = -1;
		queuedPressY = -1;
		return true;
	}

	union REGS inreg, outreg;
	inreg.x.ax = 5;
	inreg.x.bx = 0;
	int86(0x33, &inreg, &outreg);
	x = outreg.x.cx;
	y = outreg.x.dx;

	if (Platform::video->screenWidth == 320)
	{
		x /= 2;
	}

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

	if (Platform::video->screenWidth == 320)
	{
		x /= 2;
	}

	return (outreg.x.bx > 0);
}


void DOSInputDriver::HideMouse()
{
	if (!hasMouse)
		return;

	mouseHideCount++;
	if (mouseHideCount > 1)
		return;

	if (useMouseDriverCursor)
	{
		union REGS inreg, outreg;
		inreg.x.ax = 2;
		int86(0x33, &inreg, &outreg);
	}
	else
	{
		Platform::video->drawSurface->HideCursor();
	}

	mouseVisible = false;
	lastMouseX = -1;
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
	if (cursor && useMouseDriverCursor)
	{
		SetMouseCursorASM(cursor->data, cursor->hotSpotX, cursor->hotSpotY);
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

bool DOSInputDriver::HasInputPending()
{
	if (kbhit())
		return true;

	int x, y;

	if (GetMouseButtonPress(x, y))
	{
		queuedPressX = x;
		queuedPressY = y;
		return true;
	}

	return false;
}

void DOSInputDriver::RefreshMouse()
{
	if(mouseVisible && !useMouseDriverCursor)
	{
		int mouseButtons, mouseX, mouseY;
		GetMouseStatus(mouseButtons, mouseX, mouseY);

		if (mouseX != lastMouseX || mouseY != lastMouseY)
		{
			Platform::video->drawSurface->DrawCursor(Assets.GetMouseCursorData(currentCursor), mouseX, mouseY);
			lastMouseX = mouseX;
			lastMouseY = mouseY;
		}
	}
}
