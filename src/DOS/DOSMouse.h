#pragma once
#include "../Platform.h"

class DOSMouseDriver : public MouseDriver
{
public:
	virtual void Init();
	virtual void Shutdown();

	virtual void Hide();
	virtual void Show();
	virtual void SetCursor(MouseCursor::Type type);

	virtual void GetMouseState(int& buttons, int& x, int& y);

private:
	MouseCursor::Type currentCursor;
};
