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
		KeyPress
	};

	Event(App& inApp, Type inType) : app(inApp), type(inType), key(0) {}
	Event(App& inApp, Type inType, InputButtonCode inKey) : app(inApp), type(inType), key(inKey) {}

	App& app;
	Type type;
	InputButtonCode key;
};

#endif
