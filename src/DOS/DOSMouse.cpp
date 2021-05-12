#include <dos.h>
#include <stdio.h>
#include <i86.h>
#include <conio.h>
#include <memory.h>
#include <stdint.h>
#include "DOSMouse.h"

void DOSMouseDriver::Init()
{
	union REGS inreg, outreg;
	inreg.x.ax = 0;
	int86(0x33, &inreg, &outreg);

	currentCursor = MouseCursor::Hand;
	SetCursor(MouseCursor::Pointer);
}

void DOSMouseDriver::Shutdown()
{
	Hide();
}

void DOSMouseDriver::Show()
{
	union REGS inreg, outreg;
	inreg.x.ax = 1;
	int86(0x33, &inreg, &outreg);
}

void DOSMouseDriver::Hide()
{
	union REGS inreg, outreg;
	inreg.x.ax = 2;
	int86(0x33, &inreg, &outreg);
}

static void SetMouseCursor(unsigned short far* data, uint16_t hotSpotX, uint16_t hotSpotY);
#pragma aux SetMouseCursor = \
	"mov ax, 9" \
	"int 0x33" \
	modify [ax] \
	parm [es dx] [bx] [cx] 


void DOSMouseDriver::SetCursor(MouseCursor::Type type)
{
	if (type == currentCursor)
	{
		return;
	}
	MouseCursorData* cursor = Platform::video->GetCursorGraphic(type);
	SetMouseCursor(cursor->data, cursor->hotSpotX, cursor->hotSpotY);
	currentCursor = type;
}

void DOSMouseDriver::GetMouseState(int& buttons, int& x, int& y)
{
	union REGS inreg, outreg;
	inreg.x.ax = 3;
	int86(0x33, &inreg, &outreg);
	x = outreg.x.cx;
	y = outreg.x.dx;
	buttons = outreg.x.bx;
}
