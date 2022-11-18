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

#ifndef _INTERFACE_H_
#define _INTERFACE_H_
#include "Widget.h"
#include "Platform.h"
#include "URL.h"

#define NUM_APP_INTERFACE_WIDGETS 4

class App;

class AppInterface
{
public:
	AppInterface(App& inApp);

	void Reset();
	void Update();

	void DrawInterfaceWidgets();
	void UpdateAddressBar(const URL& url);

	void UpdatePageScrollBar();
	Widget* GetActiveWidget() { return activeWidget; }
	int GetTextFieldCursorPosition() {	return textFieldCursorPosition;	}

	Widget scrollBar;
	Widget addressBar;
	Widget backButton;
	Widget forwardButton;

	Widget titleBar;
	Widget statusBar;

private:
	void GenerateWidgets();
	Widget* PickWidget(int x, int y);
	void HandleClick(int mouseX, int mouseY);
	void HandleRelease();
	bool HandleActiveWidget(InputButtonCode keyPress);
	void SubmitForm(WidgetFormData* form);
	void CycleWidgets(int direction);
	void InvertWidgetsWithLinkURL(const char* url);
	void HandleButtonClicked(Widget* widget);

	void ActivateWidget(Widget* widget);
	void DeactivateWidget();

	Widget* appInterfaceWidgets[NUM_APP_INTERFACE_WIDGETS];
	App& app;

	Widget* activeWidget;
	Widget* hoverWidget;
	int oldButtons;
	int oldMouseX, oldMouseY;
	URL addressBarURL;
	int scrollBarRelativeClickPositionX;
	int scrollBarRelativeClickPositionY;
	int oldPageHeight;
	int textFieldCursorPosition;
	bool clickingButton;

	ButtonWidgetData backButtonData;
	ButtonWidgetData forwardButtonData;
	TextFieldWidgetData addressBarData;
	ScrollBarData scrollBarData;
};

#endif
