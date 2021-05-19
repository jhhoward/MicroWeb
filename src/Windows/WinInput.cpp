#include <Windows.h>
#include "WinInput.h"
#include "WinVid.h"
#include "../KeyCodes.h"

extern HWND hWnd;

static HCURSOR  hCursorHand;
static HCURSOR  hCursorArrow;

void WindowsInputDriver::Init()
{
	hCursorHand = LoadCursor(NULL, IDC_HAND);
	hCursorArrow = LoadCursor(NULL, IDC_ARROW);
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
	default:
		::SetCursor(hCursorArrow);
		break;
	}

	currentMouseCursor = type;
}

void WindowsInputDriver::GetMouseStatus(int& buttons, int& x, int& y)
{
	x = y = 0;

	POINT p;
	if (GetCursorPos(&p))
	{
		if (ScreenToClient(hWnd, &p))
		{
			x = p.x;
			y = p.y / static_cast<WindowsVideoDriver*>(Platform::video)->verticalScale;
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
	case VK_LBUTTON:
		return KEYCODE_MOUSE_LEFT;
	case VK_RBUTTON:
		return KEYCODE_MOUSE_RIGHT;
	case VK_ESCAPE:
		return KEYCODE_ESCAPE;
	case VK_UP:
		return KEYCODE_ARROW_UP;
	case VK_DOWN:
		return KEYCODE_ARROW_DOWN;
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
	default:
		return 0;
	}
}