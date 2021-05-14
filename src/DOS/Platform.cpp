#include "../Platform.h"
#include "CGA.h"
#include "DOSMouse.h"
#include "DOSNet.h"

static CGADriver cga;
static DOSMouseDriver DOSMouse;
static DOSNetworkDriver DOSNet;

VideoDriver* Platform::video = &cga;
MouseDriver* Platform::mouse = &DOSMouse;
NetworkDriver* Platform::network = &DOSNet;

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

