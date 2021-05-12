#include <stdio.h>
#include <conio.h>
#include "Renderer.h"
#include "Parser.h"
#include "Platform.h"
#include "Page.h"

#pragma warning(disable:4996)

Page page;

class PageDataProvider
{
public:
	virtual void Update() = 0;
	virtual bool IsComplete() = 0;
};

class FileDataProvider : public PageDataProvider
{
public:
	FileDataProvider(HTMLParser& inParser, FILE* inFileStream) : parser(inParser), fileStream(inFileStream) {}

	virtual bool IsComplete() { return feof(fileStream); }
	virtual void Update()
	{
		char buffer[50];

		if(!feof(fileStream))
		{
			size_t bytesRead = fread(buffer, 1, 50, fileStream);
			if (bytesRead)
			{
				parser.Parse(buffer, bytesRead);
			}
		}
	}

private:
	HTMLParser& parser;
	FILE* fileStream;
};

void ParseFile(const char* filename)
{
	FILE* fs = fopen(filename, "r");
	
	if(fs)
	{
		HTMLRenderer renderer;
		HTMLParser parser(page, renderer);
		char buffer[50];
	
		while(!feof(fs))
		{
			size_t bytesRead = fread(buffer, 1, 50, fs);
			if(!bytesRead)
			{
				break;
			}
			parser.Parse(buffer, bytesRead);
		}
	
		fclose(fs);
	}
	else
	{
		fprintf(stderr, "Error opening %s\n", filename);
	}
}

HTMLRenderer renderer;
HTMLParser parser(page, renderer);

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

	Platform::Init();

	FileDataProvider dataProvider(parser, fs);
	//ParseFile(argv[1]);
	
	while (1)
	{
		dataProvider.Update();

		int buttons, x, y;
		Platform::mouse->GetMouseState(buttons, x, y);
		if (buttons == 2)
		{
			break;
		}
		if (buttons == 1)
		{
			Platform::mouse->SetCursor(MouseCursor::Hand);
		}
		else
		{
			Platform::mouse->SetCursor(MouseCursor::Pointer);
		}

		if (kbhit())
		{
			int keyPress = getch();
			if (keyPress == 27)
			{
				break;
			}
			else
			{
				if (keyPress == 0)
				{
					keyPress = getch();
					printf("special %d\n", keyPress);
				}
				else
				{
					if (keyPress >= 32 && keyPress < 128)
					{
						printf("%d - '%c'\n", keyPress, (char)keyPress);
					}
					else
					{
						printf("%d\n", keyPress);
					}
				}
			}
		}
	}

	Platform::Shutdown();

	return 0;
}
