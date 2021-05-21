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

class App;
struct Widget;

class Renderer
{
public:
	Renderer(App& inApp);

	void Init();
	void Reset();
	void Update();
	void Scroll(int delta);

	void RedrawScrollBar();

	Widget* PickPageWidget(int x, int y);
	bool IsOverPageWidget(Widget* widget, int x, int y);

	void DrawStatus(const char* status);
	void DrawAddress(const char* address);

	void RenderPageWidget(Widget* widget);
	void RenderWidget(Widget* widget, int baseY = 0);
private:
	void DrawButtonRect(int x, int y, int width, int height);

	int GetMaxScrollPosition();

	App& app;
	int scrollPosition;
	int pageTopWidgetIndex;
	int oldPageHeight;
	const char* oldStatus;
	int upperRenderLine, lowerRenderLine;
};

