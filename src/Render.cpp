#include "Render.h"
#include "Node.h"
#include "App.h"
#include "Interface.h"
#include "Draw/Surface.h"
#include "DataPack.h"

PageRenderer::PageRenderer(App& inApp)
	: app(inApp)
{

}

void PageRenderer::Init()
{

}

void PageRenderer::Reset()
{
}

void PageRenderer::Update()
{

}	

void PageRenderer::DrawAll(DrawContext& context, Node* node)
{
	while (node)
	{
		node->Handler().Draw(context, node);

		DrawAll(context, node->firstChild);

		node = node->next;
	}

	context.surface->BlitImage(context, Assets.imageIcon, 0, 50);
	context.surface->BlitImage(context, Assets.imageIcon, 1, 100);
	context.surface->BlitImage(context, Assets.imageIcon, 2, 150);
}

void PageRenderer::GenerateDrawContext(DrawContext& context, Node* node)
{
	context.surface = Platform::video->drawSurface;

	if (app.ui.IsInterfaceNode(node))
	{
		context.clipLeft = 0;
		context.clipRight = Platform::video->screenWidth;
		context.clipTop = 0;
		context.clipBottom = Platform::video->screenHeight;
		context.drawOffsetX = 0;
		context.drawOffsetY = 0;
	}
	else
	{
		Rect& windowRect = app.ui.windowRect;
		context.clipLeft = windowRect.x;
		context.clipRight = windowRect.x + windowRect.width;
		context.clipTop = windowRect.y;
		context.clipBottom = windowRect.y + windowRect.height;
		context.drawOffsetX = windowRect.x;
		context.drawOffsetY = windowRect.y - app.ui.GetScrollPositionY();
	}
}
