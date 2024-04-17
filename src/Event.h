#pragma once
#ifndef _EVENT_H_
#define _EVENT_H_

#include "Platform.h"

class App;

struct Event
{
	enum Type
	{
		MouseClick,
		MouseRelease,
		MouseDrag,
		KeyPress,
		Focus,
		Unfocus
	};

	Event(App& inApp, Type inType) : app(inApp), type(inType), key(0), x(0), y(0) {}
	Event(App& inApp, Type inType, InputButtonCode inKey) : app(inApp), type(inType), key(inKey), x(0), y(0) {}
	Event(App& inApp, Type inType, int inX, int inY) : app(inApp), type(inType), key(0), x(inX), y(inY) {}

	App& app;
	Type type;
	InputButtonCode key;
	int x, y;
};

#endif
