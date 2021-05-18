#include "../Platform.h"
#include "CGA.h"
#include "DOSInput.h"
#include "DOSNet.h"

static CGADriver cga;
static DOSInputDriver DOSinput;
static DOSNetworkDriver DOSNet;

VideoDriver* Platform::video = &cga;
InputDriver* Platform::input = &DOSinput;
NetworkDriver* Platform::network = &DOSNet;

void Platform::Init()
{
	network->Init();
	video->Init();
	video->ClearScreen();
	input->Init();
}

void Platform::Shutdown()
{
	input->Shutdown();
	video->Shutdown();
	network->Shutdown();
}

void Platform::Update()
{
	network->Update();
	input->Update();
}
