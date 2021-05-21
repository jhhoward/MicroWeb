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
