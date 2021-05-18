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
	virtual InputButtonCode GetKeyPress();

private:
	MouseCursor::Type currentCursor;
	int lastMouseButtons;
};
