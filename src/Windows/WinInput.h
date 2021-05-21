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

#include <vector>
#include "../Platform.h"

class WindowsInputDriver : public InputDriver
{
public:
	virtual void Init();

	virtual void ShowMouse();
	virtual void HideMouse();

	virtual void SetMouseCursor(MouseCursor::Type type);
	virtual void GetMouseStatus(int& buttons, int& x, int& y);
	void RefreshCursor();
	virtual InputButtonCode GetKeyPress();

	void QueueCharPress(char code);
	void QueueKeyPress(WPARAM code);

private:
	InputButtonCode TranslateCode(WPARAM code);

	MouseCursor::Type currentMouseCursor;
	std::vector<InputButtonCode> inputQueue;
};
