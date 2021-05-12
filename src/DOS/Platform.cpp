#include "../Platform.h"
#include "CGA.h"
#include "DOSMouse.h"

static CGADriver cga;
static DOSMouseDriver DOSMouse;

VideoDriver* Platform::video = &cga;
MouseDriver* Platform::mouse = &DOSMouse;

void Platform::Init()
{
	video->Init();
	video->ClearScreen();
	mouse->Init();
	mouse->Show();
}

void Platform::Shutdown()
{
	mouse->Shutdown();
	video->Shutdown();
}

