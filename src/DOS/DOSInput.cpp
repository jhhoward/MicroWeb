#include <dos.h>
#include <stdio.h>
#include <i86.h>
#include <conio.h>
#include <memory.h>
#include <stdint.h>
#include "DOSInput.h"
#include "../Keycodes.h"

void DOSInputDriver::Init()
{
	union REGS inreg, outreg;
	inreg.x.ax = 0;
	int86(0x33, &inreg, &outreg);

	currentCursor = MouseCursor::Hand;
	SetMouseCursor(MouseCursor::Pointer);
	lastMouseButtons = 0;

	ShowMouse();
}

void DOSInputDriver::Shutdown()
{
	HideMouse();
}

void DOSInputDriver::ShowMouse()
{
	union REGS inreg, outreg;
	inreg.x.ax = 1;
	int86(0x33, &inreg, &outreg);
}

void DOSInputDriver::HideMouse()
{
	union REGS inreg, outreg;
	inreg.x.ax = 2;
	int86(0x33, &inreg, &outreg);
}

static void SetMouseCursorASM(unsigned short far* data, uint16_t hotSpotX, uint16_t hotSpotY);
#pragma aux SetMouseCursorASM = \
	"mov ax, 9" \
	"int 0x33" \
	modify [ax] \
	parm [es dx] [bx] [cx] 


void DOSInputDriver::SetMouseCursor(MouseCursor::Type type)
{
	if (type == currentCursor)
	{
		return;
	}
	MouseCursorData* cursor = Platform::video->GetCursorGraphic(type);
	SetMouseCursorASM(cursor->data, cursor->hotSpotX, cursor->hotSpotY);
	currentCursor = type;
}

void DOSInputDriver::GetMouseStatus(int& buttons, int& x, int& y)
{
	union REGS inreg, outreg;
	inreg.x.ax = 3;
	int86(0x33, &inreg, &outreg);
	x = outreg.x.cx;
	y = outreg.x.dx;
	buttons = outreg.x.bx;
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

	int buttons, mouseX, mouseY;
	GetMouseStatus(buttons, mouseX, mouseY);

	int oldButtons = lastMouseButtons;
	lastMouseButtons = buttons;

	if (buttons != oldButtons)
	{
		if ((buttons & 1) && !(oldButtons & 1))
		{
			return KEYCODE_MOUSE_LEFT;
		}
		if ((buttons & 2) && !(oldButtons & 2))
		{
			return KEYCODE_MOUSE_RIGHT;
		}
	}

	return 0;
}
