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

#include "../Platform.h"
#include "CGA.h"
#include "Hercules.h"
#include "DOSInput.h"
#include "DOSNet.h"

static CGADriver cga;
//static HerculesDriver hercules;
static DOSInputDriver DOSinput;
static DOSNetworkDriver DOSNet;

//VideoDriver* Platform::video = &cga;
VideoDriver* Platform::video = &hercules;
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
