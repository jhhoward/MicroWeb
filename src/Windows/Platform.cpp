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

#include <windows.h>
#include <stdarg.h>
#include "../Platform.h"
#include "WinVid.h"
#include "WinInput.h"
#include "WinNet.h"
#include "../Draw/Surface.h"
#include "../VidModes.h"
#include "../Memory/Memory.h"
#include "../App.h"

WindowsVideoDriver winVid;
WindowsNetworkDriver winNetworkDriver;
WindowsInputDriver winInputDriver;

VideoDriver* Platform::video = &winVid;
NetworkDriver* Platform::network = &winNetworkDriver;
InputDriver* Platform::input = &winInputDriver;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
HWND hWnd;

bool Platform::Init(int argc, char* argv[])
{
	VideoModeInfo* videoMode = ShowVideoModePicker(8);
	if (!videoMode)
	{
		return false;
	}

	network->Init();
	video->Init(videoMode);
	input->Init();
	input->ShowMouse();

	return true;
}

void Platform::Shutdown()
{
	MemoryManager::pageBlockAllocator.Shutdown();
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

	App& app = App::Get();
	if (!app.pageRenderer.IsRendering() && !app.pageLoadTask.IsBusy() && !app.pageContentLoadTask.IsBusy() && app.page.layout.IsFinished())
	{
		Sleep(10);
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
		if ((int)wParam > 0)
		{
			winInputDriver.QueueKeyPress(VK_UP);
		}
		else 
		{
			winInputDriver.QueueKeyPress(VK_DOWN);
		}
		break;
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Platform::FatalError(const char* message, ...)
{
	va_list args;

	if (video)
	{
		video->Shutdown();
	}

	va_start(args, message);

	size_t size = vsnprintf(NULL, 0, message, args);

	// Allocate memory for the message
	char* buffer = new char[size + 1];

	// Format the message
	vsnprintf(buffer, size + 1, message, args);

	va_end(args);

	int wsize = MultiByteToWideChar(CP_ACP, 0, buffer, -1, NULL, 0);
	wchar_t* wbuffer = new wchar_t[wsize + 1];
	MultiByteToWideChar(CP_ACP, 0, buffer, -1, wbuffer, wsize);

	MessageBox(NULL, wbuffer, L"Fatal error", MB_OK);

	delete[] wbuffer;
	delete[] buffer;

	exit(1);
}
