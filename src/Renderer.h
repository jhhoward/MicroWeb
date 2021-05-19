#pragma once

class App;
struct Widget;

class Renderer
{
public:
	Renderer(App& inApp);

	void Reset();
	void Update();
	void Scroll(int delta);

	void OnPageWidgetsLoaded(Widget* widget, int count);
	void RedrawScrollBar();

	Widget* PickPageWidget(int x, int y);
	bool IsOverPageWidget(Widget* widget, int x, int y);

	void DrawStatus(const char* status);
	void DrawAddress(const char* address);

private:
	void DrawButtonRect(int x, int y, int width, int height);

	void RenderWidget(Widget* widget);
	int GetMaxScrollPosition();

	App& app;
	int scrollPosition;
	int pageTopWidgetIndex;
	int oldPageHeight;
	const char* oldStatus;
	int upperRenderLine, lowerRenderLine;
};

