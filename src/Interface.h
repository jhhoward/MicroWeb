#pragma once
#include "Widget.h"

#define NUM_APP_INTERFACE_WIDGETS 4

class App;

class AppInterface
{
public:
	AppInterface(App& inApp);

	void Update();

	void DrawInterfaceWidgets();

	Widget scrollBar;
	Widget addressBar;
	Widget backButton;
	Widget forwardButton;

private:
	void GenerateWidgets();
	Widget* PickWidget(int x, int y);
	bool IsOverWidget(Widget* widget, int x, int y);

	Widget* appInterfaceWidgets[NUM_APP_INTERFACE_WIDGETS];
	App& app;

	Widget* hoverWidget;
	int oldMouseX, oldMouseY;

	ButtonWidgetData backButtonData;
	ButtonWidgetData forwardButtonData;
	TextFieldWidgetData addressBarData;
};
