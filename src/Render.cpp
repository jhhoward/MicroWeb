#include "Render.h"
#include "Node.h"
#include "App.h"
#include "Interface.h"
#include "Draw/Surface.h"

PageRenderer::PageRenderer(App& inApp)
	: app(inApp), scrollPositionY(0)
{

}

void PageRenderer::Init()
{

}

void PageRenderer::Reset()
{
	scrollPositionY = 0;
}

void PageRenderer::Update()
{

}	

void PageRenderer::ScrollRelative(int delta)
{
	scrollPositionY += delta;
	if (scrollPositionY < 0)
		scrollPositionY = 0;
	
	int maxScrollY = app.page.GetRootNode()->size.y - app.ui.windowRect.height;
	if (maxScrollY > 0 && scrollPositionY > maxScrollY)
	{
		scrollPositionY = maxScrollY;
	}

	DrawContext context;
	GenerateDrawContext(context, NULL);
	context.drawOffsetY = 0;
	context.surface->FillRect(context, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, 1);
	GenerateDrawContext(context, NULL);
	DrawAll(context, app.page.GetRootNode());
}

void PageRenderer::ScrollAbsolute(int position)
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
		context.drawOffsetY = windowRect.y - scrollPositionY;
	}
}
