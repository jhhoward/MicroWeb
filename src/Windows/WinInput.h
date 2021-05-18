#pragma once

#include <vector>
#include "../Platform.h"

class WindowsInputDriver : public InputDriver
{
public:
	virtual void Init();

	virtual void ShowMouse() {}
	virtual void HideMouse() {}

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
