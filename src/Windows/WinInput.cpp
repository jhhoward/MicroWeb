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

#include <Windows.h>
#include "WinInput.h"
#include "WinVid.h"
#include "../KeyCodes.h"

extern HWND hWnd;

static HCURSOR  hCursorHand;
static HCURSOR  hCursorArrow;
static HCURSOR  hCursorTextSelect;

void WindowsInputDriver::Init()
{
	hCursorHand = LoadCursor(NULL, IDC_HAND);
	hCursorArrow = LoadCursor(NULL, IDC_ARROW);
	hCursorTextSelect = LoadCursor(NULL, IDC_IBEAM);
	currentMouseCursor = MouseCursor::Pointer;
}

void WindowsInputDriver::HideMouse()
{

}

void WindowsInputDriver::ShowMouse()
{

}

void WindowsInputDriver::SetMouseCursor(MouseCursor::Type type)
{
	switch (type)
	{
	case MouseCursor::Hand:
		::SetCursor(hCursorHand);
		break;
	case MouseCursor::TextSelect:
		::SetCursor(hCursorTextSelect);
		break;
	default:
		::SetCursor(hCursorArrow);
		break;
	}

	currentMouseCursor = type;
}

void WindowsInputDriver::SetMousePosition(int x, int y)
{
	POINT p;
	p.x = x;
	p.y = y * static_cast<WindowsVideoDriver*>(Platform::video)->verticalScale;
	if (ClientToScreen(hWnd, &p))
	{
		SetCursorPos(p.x, p.y);
	}
}

void WindowsInputDriver::GetMouseStatus(int& buttons, int& x, int& y)
{
	x = y = 0;

	POINT p;
	if (GetCursorPos(&p))
	{
		if (ScreenToClient(hWnd, &p))
		{
			// Calculate the destination rectangle to stretch the bitmap to fit the window
			RECT windowRect;
			GetClientRect(hWnd, &windowRect);

			int width = windowRect.right - windowRect.left;
			int height = windowRect.bottom - windowRect.top;

			if (width > 0 && height > 0)
			{
				x = p.x * Platform::video->screenWidth / width;
				y = p.y * Platform::video->screenHeight / height;
			}
		}
	}

	if (GetKeyState(VK_LBUTTON) < 0)
	{
		buttons |= 1;
	}
	if (GetKeyState(VK_RBUTTON) < 0)
	{
		buttons |= 2;
	}
}

void WindowsInputDriver::RefreshCursor()
{
	SetMouseCursor(currentMouseCursor);
}

InputButtonCode WindowsInputDriver::GetKeyPress()
{
	if (inputQueue.size())
	{
		InputButtonCode result = inputQueue[0];
		inputQueue.erase(inputQueue.begin());
		return result;
	}
	return 0;
}

void WindowsInputDriver::QueueKeyPress(WPARAM code)
{
	InputButtonCode translated = TranslateCode(code);

	if (translated)
	{
		inputQueue.push_back(translated);
	}
}

void WindowsInputDriver::QueueCharPress(char code)
{
	if (code >= 32 && code < 128)
	{
		inputQueue.push_back(code);
	}
}

InputButtonCode WindowsInputDriver::TranslateCode(WPARAM code)
{
	switch (code)
	{
	//case VK_LBUTTON:
	//	return KEYCODE_MOUSE_LEFT;
	//case VK_RBUTTON:
	//	return KEYCODE_MOUSE_RIGHT;
	case VK_ESCAPE:
		return KEYCODE_ESCAPE;
	case VK_UP:
		return KEYCODE_ARROW_UP;
	case VK_DOWN:
		return KEYCODE_ARROW_DOWN;
	case VK_LEFT:
		return KEYCODE_ARROW_LEFT;
	case VK_RIGHT:
		return KEYCODE_ARROW_RIGHT;
	case VK_HOME:
		return KEYCODE_HOME;
	case VK_END:
		return KEYCODE_END;
	case VK_PRIOR:
		return KEYCODE_PAGE_UP;
	case VK_NEXT:
		return KEYCODE_PAGE_DOWN;
	case VK_RETURN:
		return KEYCODE_ENTER;
	case VK_DELETE:
		return KEYCODE_DELETE;
	case VK_BACK:
		return KEYCODE_BACKSPACE;
	case VK_TAB:
		if (GetKeyState(VK_SHIFT) & 0x8000)
			return KEYCODE_SHIFT_TAB;
		else
			return KEYCODE_TAB;
	case VK_F1:
		return KEYCODE_F1;
	case VK_F2:
		return KEYCODE_F2;
	case VK_F3:
		return KEYCODE_F3;
	case VK_F4:
		return KEYCODE_F4;
	case VK_F5:
		return KEYCODE_F5;
	case VK_F6:
		return KEYCODE_F6;
	case VK_F7:
		return KEYCODE_F7;
	case VK_F8:
		return KEYCODE_F8;
	case VK_F9:
		return KEYCODE_F9;
	case VK_F10:
		return KEYCODE_F10;
	default:
		return 0;
	}
}