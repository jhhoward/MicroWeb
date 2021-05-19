#include <windows.h>
#include "../Platform.h"
#include "WinVid.h"
#include "WinInput.h"

WindowsVideoDriver winVid;
NetworkDriver nullNetworkDriver;
WindowsInputDriver winInputDriver;

VideoDriver* Platform::video = &winVid;
NetworkDriver* Platform::network = &nullNetworkDriver;
InputDriver* Platform::input = &winInputDriver;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HWND hWnd;

void Platform::Init()
{
	WNDCLASSW wc = { 0 };
	HINSTANCE hInstance = GetModuleHandle(NULL);

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpszClassName = L"Pixels";
	wc.hInstance = hInstance;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc = WndProc;
	wc.hCursor = LoadCursor(0, IDC_ARROW);

	RECT wr = { 0, 0, winVid.screenWidth, winVid.screenHeight * winVid.verticalScale };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	RegisterClassW(&wc);
	hWnd = CreateWindowW(wc.lpszClassName, L"MicroWeb",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		100, 100, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInstance, NULL);

	network->Init();
	video->Init();
	video->ClearScreen();
	input->Init();
	input->ShowMouse();
}

void Platform::Shutdown()
{
	input->Shutdown();
	video->Shutdown();
	network->Shutdown();
}

void Platform::Update()
{
	network->Update();

	InvalidateRect(hWnd, NULL, NULL);
	MSG  msg;

	if (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	else
	{
		Shutdown();
		exit(0);
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
	WPARAM wParam, LPARAM lParam) {

	switch (msg) {

	case WM_PAINT:
		static_cast<WindowsVideoDriver*>(Platform::video)->Paint(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			winInputDriver.RefreshCursor();
			return TRUE;
		}
		break;

	case WM_KEYDOWN:
		winInputDriver.QueueKeyPress(wParam);
		break;

	case WM_LBUTTONDOWN:
		winInputDriver.QueueKeyPress(VK_LBUTTON);
		break;

	case WM_RBUTTONDOWN:
		winInputDriver.QueueKeyPress(VK_RBUTTON);
		break;

	case WM_CHAR:
		winInputDriver.QueueCharPress((char)wParam);
		break;

	case WM_MOUSEWHEEL:
		if (wParam < 0)
		{
			winInputDriver.QueueKeyPress(VK_UP);
		}
		else if (wParam > 0)
		{
			winInputDriver.QueueKeyPress(VK_DOWN);
		}
		break;
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}
