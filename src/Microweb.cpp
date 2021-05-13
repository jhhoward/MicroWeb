#include <stdio.h>
#include "Platform.h"
#include "App.h"

#pragma warning(disable:4996)

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s [file]\n", argv[0]);
		return 0;
	}

	FILE* fs = fopen(argv[1], "r");
	if (!fs)
	{
		fprintf(stderr, "Error opening %s\n", argv[1]);
		return 0;
	}
	fclose(fs);

	App* app = new App();

	if (!app)
	{
		fprintf(stderr, "Not enough memory\n");
		return 0;
	}

	Platform::Init();

	app->OpenURL(argv[1]);
	app->Run();

	Platform::Shutdown();

	return 0;
}
