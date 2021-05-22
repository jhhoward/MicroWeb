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
	void ScrollTo(int position);

	void RedrawScrollBar();
	int GetMaxScrollPosition();
	int GetScrollPosition() { return scrollPosition; }

	Widget* PickPageWidget(int x, int y);
	bool IsOverWidget(Widget* widget, int x, int y);

	void SetStatus(const char* status);
	void SetTitle(const char* status);

	void DrawAddress(const char* address);

	void RenderWidget(Widget* widget);
	void InvertWidget(Widget* widget);
private:
	void DrawButtonRect(int x, int y, int width, int height);
	void RenderWidgetInternal(Widget* widget, int baseY);

	App& app;
	int scrollPosition;
	int pageTopWidgetIndex;
	const char* oldStatus;
	int upperRenderLine, lowerRenderLine;
};

