#include "Render.h"
#include "Node.h"
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

