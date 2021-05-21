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

#include <stdio.h>
#include "Platform.h"
#include "App.h"

#pragma warning(disable:4996)

int main(int argc, char* argv[])
{
	App* app = new App();

	if (!app)
	{
		fprintf(stderr, "Not enough memory\n");
		return 0;
	}

	Platform::Init();

	if (argc >= 2)
	{
		app->OpenURL(argv[1]);
	}

	app->Run();

	Platform::Shutdown();

	return 0;
}
