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
	Reset();
}

void PageRenderer::InitContext(DrawContext& context)
{
	context.surface = Platform::video->drawSurface;

	Rect& windowRect = app.ui.windowRect;
	context.clipLeft = windowRect.x;
	context.clipRight = windowRect.x + windowRect.width;
	context.clipTop = windowRect.y;
	context.clipBottom = windowRect.y;
	context.drawOffsetX = windowRect.x;
	context.drawOffsetY = windowRect.y;
}

void PageRenderer::ClampContextToRect(DrawContext& context, Rect& rect)
{
	if (context.clipTop < rect.y)
		context.clipTop = rect.y;
	if (context.clipBottom < rect.y)
		context.clipBottom = rect.y;

	int maxY = rect.y + rect.height;
	if (context.clipTop > maxY)
		context.clipTop = maxY;
	if (context.clipBottom > maxY)
		context.clipBottom = maxY;
}

void PageRenderer::RefreshAll()
{
	Platform::input->HideMouse();

	Rect& windowRect = app.ui.windowRect;

	DrawContext clearContext;
	InitContext(clearContext);
	clearContext.clipBottom = windowRect.y + windowRect.height;
	clearContext.surface->FillRect(clearContext, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, Platform::video->colourScheme.pageColour);

	Platform::input->ShowMouse();

	upperContext.clipBottom = upperContext.clipTop;
	lowerContext.clipTop = upperContext.clipBottom;

	// Clear the render queue
	for (Node* node = renderQueueHead; node;)
	{
		Node* next = node->nextNodeToRender;
		node->nextNodeToRender = nullptr;
		node = next;
	}
	renderQueueHead = renderQueueTail = nullptr;

	FindOverlappingNodesInScreenRegion(windowRect.y, windowRect.y + windowRect.height);
}

void PageRenderer::OnPageScroll(int scrollDelta)
{
	if (scrollDelta == 0)
	{
		return;
	}

	Rect& windowRect = app.ui.windowRect;
	upperContext.drawOffsetY = windowRect.y - app.ui.GetScrollPositionY();
	lowerContext.drawOffsetY = windowRect.y - app.ui.GetScrollPositionY();

	int minWinY = windowRect.y;
	int maxWinY = windowRect.y + windowRect.height;

	if (scrollDelta > 0)
	{
		lowerContext.clipTop -= scrollDelta;
		if (lowerContext.clipTop < minWinY)
			lowerContext.clipTop = minWinY;

		upperContext.clipBottom -= scrollDelta;
		if (upperContext.clipBottom < minWinY)
			upperContext.clipBottom = minWinY;

		if (lowerContext.clipTop < upperContext.clipBottom)
		{
			lowerContext.clipTop = upperContext.clipBottom;
		}

		int top = maxWinY - scrollDelta;
		if (top < minWinY)
			top = minWinY;
		FindOverlappingNodesInScreenRegion(top, maxWinY);
	}
	else if (scrollDelta < 0)
	{
		upperContext.clipBottom -= scrollDelta;
		if (upperContext.clipBottom > maxWinY)
			upperContext.clipBottom = maxWinY;
		
		lowerContext.clipTop -= scrollDelta;
		if (lowerContext.clipTop > maxWinY)
			lowerContext.clipTop = maxWinY;

		if (upperContext.clipBottom > lowerContext.clipTop)
		{
			upperContext.clipBottom = lowerContext.clipTop;
		}

		int bottom = minWinY - scrollDelta;
		if (bottom > maxWinY)
			bottom = maxWinY;
		FindOverlappingNodesInScreenRegion(minWinY, bottom);
	}

	Platform::input->HideMouse();

	DrawContext clearContext;
	InitContext(clearContext);
	
	if (scrollDelta > 0)
	{
		Platform::video->drawSurface->ScrollScreen(minWinY, maxWinY - scrollDelta, windowRect.width, scrollDelta);
		clearContext.clipTop = maxWinY - scrollDelta;
		if (clearContext.clipTop < minWinY)
		{
			clearContext.clipTop = minWinY;
		}
		clearContext.clipBottom = maxWinY;
		clearContext.surface->FillRect(clearContext, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, Platform::video->colourScheme.pageColour);
	}
	else
	{
		Platform::video->drawSurface->ScrollScreen(minWinY - scrollDelta, maxWinY, windowRect.width, scrollDelta);
		clearContext.clipTop = minWinY;
		clearContext.clipBottom = minWinY - scrollDelta;
		if (clearContext.clipBottom > maxWinY)
		{
			clearContext.clipBottom = maxWinY;
		}
		clearContext.surface->FillRect(clearContext, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, Platform::video->colourScheme.pageColour);
	}

	Platform::input->ShowMouse();
}

bool PageRenderer::IsRenderableNode(Node::Type type)
{
	switch (type)
	{
	case Node::Style:
	case Node::Section:
	case Node::Link:
	case Node::Form:
		return false;
	default:
		return true;
	}
}

void PageRenderer::FindOverlappingNodesInScreenRegion(int top, int bottom)
{
	int drawOffsetY = upperContext.drawOffsetY;

	if (!lastCompleteNode)
		return;

	for(Node* node = app.page.GetRootNode(); node; node = node->GetNextInTree())
	{
		if(IsRenderableNode(node->type))
		{
			bool outsideOfRegion = (node->anchor.y + drawOffsetY > bottom) || (node->anchor.y + node->size.y + drawOffsetY < top);

			if (!outsideOfRegion)
			{
				AddToQueue(node);
			}
		}

		if (node == lastCompleteNode)
			break;
	}
}

void PageRenderer::Reset()
{
	renderQueueHead = nullptr;
	renderQueueTail = nullptr;
	renderQueueSize = 0;
	lastCompleteNode = nullptr;

	InitContext(upperContext);
	InitContext(lowerContext);

	Rect& windowRect = app.ui.windowRect;
	lowerContext.clipBottom = lowerContext.clipTop = windowRect.y + windowRect.height;

	visiblePageHeight = 0;
}

bool PageRenderer::DoesOverlapWithContext(Node* node, DrawContext& context)
{
	if (node->anchor.y + context.drawOffsetY > context.clipBottom)
	{
		return false;
	}
	if (node->anchor.y + node->size.y + context.drawOffsetY < context.clipTop)
	{
		return false;
	}
	return true;
}

void PageRenderer::Update()
{
//	if (rand() % 256)
//		return;

	bool renderedSomething = false;

	while(renderQueueHead && !renderedSomething)
	{
		Node* toRender = renderQueueHead;

		Platform::input->HideMouse();
		if (upperContext.clipTop != upperContext.clipBottom && DoesOverlapWithContext(toRender, upperContext))
		{
			toRender->Handler().Draw(upperContext, toRender);
			renderedSomething = true;
		}
		if (lowerContext.clipTop != lowerContext.clipBottom && DoesOverlapWithContext(toRender, lowerContext))
		{
			toRender->Handler().Draw(lowerContext, toRender);
			renderedSomething = true;
		}
		Platform::input->ShowMouse();

		renderQueueHead = toRender->nextNodeToRender;
		toRender->nextNodeToRender = nullptr;

		if (!renderQueueHead)
		{
			renderQueueTail = nullptr;
		}
		renderQueueSize--;
	}

	if (!renderQueueHead)
	{
		lowerContext.clipTop = lowerContext.clipBottom;
		upperContext.clipBottom = upperContext.clipTop;
	}
}	

void PageRenderer::AddToQueue(Node* node)
{
	if (node->nextNodeToRender || node == renderQueueTail)
	{
		// Already in queue
		return;
	}

	if (!renderQueueHead)
	{
		renderQueueTail = renderQueueHead = node;
	}
	else
	{
		renderQueueTail->nextNodeToRender = node;
		renderQueueTail = node;
	}

	renderQueueSize++;
}

bool PageRenderer::IsInRenderQueue(Node* node)
{
	return node && (node->nextNodeToRender || node == renderQueueTail);
}

void PageRenderer::DrawAll(DrawContext& context, Node* node)
{
	Platform::input->HideMouse();
	while (node)
	{
		node->Handler().Draw(context, node);

		node = node->GetNextInTree();
	}
	Platform::input->ShowMouse();

	//context.surface->BlitImage(context, Assets.imageIcon, 0, 50);
	//context.surface->BlitImage(context, Assets.imageIcon, 1, 100);
	//context.surface->BlitImage(context, Assets.imageIcon, 2, 150);
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

void PageRenderer::MarkNodeLayoutComplete(Node* node)
{
	Rect& windowRect = app.ui.windowRect;
	int drawOffsetY = upperContext.drawOffsetY;
	Node* startNode = lastCompleteNode ? lastCompleteNode->GetNextInTree() : app.page.GetRootNode();
	lastCompleteNode = node;

	int minWinY = windowRect.y;
	int maxWinY = windowRect.y + windowRect.height;

	bool expandedPage = false;

	for (Node* node = startNode; node; node = node->GetNextInTree())
	{
		if (IsRenderableNode(node->type))
		{
			int nodeTop = node->anchor.y + drawOffsetY;
			int nodeBottom = nodeTop + node->size.y;
			bool outsideOfWindow = (nodeTop > maxWinY) || (nodeBottom < minWinY);

			if (nodeBottom > visiblePageHeight)
			{
				visiblePageHeight = nodeBottom;
				expandedPage = true;
			}

			if (!outsideOfWindow)
			{
				if (upperContext.clipBottom < nodeBottom)
				{
					upperContext.clipBottom = nodeBottom;
					if (upperContext.clipBottom > maxWinY)
					{
						upperContext.clipBottom = maxWinY;
					}
				}
				if (lowerContext.clipTop < upperContext.clipBottom)
				{
					lowerContext.clipTop = upperContext.clipBottom;
				}
				AddToQueue(node);
			}
		}

		if (node == lastCompleteNode)
			break;
	}

	if (expandedPage)
	{
		App::Get().ui.UpdatePageScrollBar();
	}
}

void PageRenderer::MarkNodeDirty(Node* dirtyNode)
{
	// Check this is in a completed layout
	for (Node* node = app.page.GetRootNode(); node; node = node->GetNextInTree())
	{
		if (node == lastCompleteNode)
			return;

		if (node == dirtyNode)
			break;
	}

	Rect& windowRect = app.ui.windowRect;
	int drawOffsetY = upperContext.drawOffsetY;
	int minWinY = windowRect.y;
	int maxWinY = windowRect.y + windowRect.height;

	int nodeTop = dirtyNode->anchor.y + drawOffsetY;
	int nodeBottom = nodeTop + dirtyNode->size.y;
	bool outsideOfWindow = (nodeTop > maxWinY) || (nodeBottom < minWinY);

	if (!outsideOfWindow)
	{
		if (upperContext.clipBottom < nodeBottom)
		{
			upperContext.clipBottom = nodeBottom;
			if (upperContext.clipBottom > maxWinY)
			{
				upperContext.clipBottom = maxWinY;
			}
		}
		if (lowerContext.clipTop < upperContext.clipBottom)
		{
			lowerContext.clipTop = upperContext.clipBottom;
		}
		AddToQueue(dirtyNode);

		Platform::input->HideMouse();

		Rect& windowRect = app.ui.windowRect;

		DrawContext clearContext;
		InitContext(clearContext);
		clearContext.clipBottom = windowRect.y + windowRect.height;
		clearContext.surface->FillRect(clearContext, dirtyNode->anchor.x, dirtyNode->anchor.y, dirtyNode->size.x, dirtyNode->size.y, Platform::video->colourScheme.pageColour);

		Platform::input->ShowMouse();
	}
}

void PageRenderer::MarkPageLayoutComplete()
{
	Node* node = lastCompleteNode;
	while (node)
	{
		Node* next = node->GetNextInTree();
		if (next)
		{
			node = next;
		}
		else
		{
			MarkNodeLayoutComplete(node);
			break;
		}
	}
}