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

#pragma once
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
	virtual InputButtonCode GetKeyPress();

private:
	MouseCursor::Type currentCursor;
	int lastMouseButtons;
	bool mouseVisible;
};
