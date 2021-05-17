#include "../Platform.h"
#include "WinVid.h"

WindowsVideoDriver winVid;
NetworkDriver nullNetworkDriver;
MouseDriver nullMouseDriver;

VideoDriver* Platform::video = &winVid;
NetworkDriver* Platform::network = &nullNetworkDriver;
MouseDriver* Platform::mouse = &nullMouseDriver;

void Platform::Init()
{
	network->Init();
	video->Init();
	video->ClearScreen();
	mouse->Init();
	mouse->Show();
}

void Platform::Shutdown()
{
	mouse->Shutdown();
	video->Shutdown();
	network->Shutdown();
}

