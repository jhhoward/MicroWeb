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

#ifndef _DOSINPUT_H_
#define _DOSINPUT_H_
#include "../Platform.h"

class DOSInputDriver : public InputDriver
{
public:
	virtual void Init();
	virtual void Shutdown();

	virtual void HideMouse();
	virtual void ShowMouse();
	virtual void SetMouseCursor(MouseCursor::Type type);

	virtual void GetMouseStatus(int& buttons, int& x, int& y);
	virtual void SetMousePosition(int x, int y);
	virtual bool GetMouseButtonPress(int& x, int& y);
	virtual bool GetMouseButtonRelease(int& x, int& y);

	virtual InputButtonCode GetKeyPress();
	virtual bool HasInputPending();

private:
	MouseCursor::Type currentCursor;
	bool mouseVisible;
	bool hasMouse;
	int mouseHideCount;

	int queuedPressX, queuedPressY;
};

#endif
