#include "Render.h"
#include "Node.h"
#include "App.h"
#include "Interface.h"
#include "Draw/Surface.h"
#include "DataPack.h"
#include "Nodes/ImgNode.h"

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
	context.clipBottom = windowRect.y + windowRect.height;
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
	clearContext.surface->FillRect(clearContext, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, app.page.colourScheme.pageColour);

	Platform::input->ShowMouse();

	renderQueue.Reset();

	FindOverlappingNodesInScreenRegion(windowRect.y, windowRect.y + windowRect.height);
}

void PageRenderer::OnPageScroll(int scrollDelta)
{
	if (scrollDelta == 0)
	{
		return;
	}

	Rect& windowRect = app.ui.windowRect;

	int minWinY = windowRect.y;
	int maxWinY = windowRect.y + windowRect.height;

	for (int i = renderQueue.head; i < renderQueue.tail; i++)
	{
		RenderQueue::Item* item = &renderQueue.items[i];
		item->lowerClip -= scrollDelta;
		item->upperClip -= scrollDelta;

		if (item->upperClip < minWinY)
			item->upperClip = minWinY;
		if (item->lowerClip > maxWinY)
			item->lowerClip = maxWinY;

		if (item->lowerClip <= item->upperClip)
		{
			// Scrolled off screen, can remove from queue
			renderQueue.tail--;
			for (int n = i; n < renderQueue.tail; n++)
			{
				renderQueue.items[n] = renderQueue.items[n + 1];
			}
			i--;
		}
	}

	if (scrollDelta > 0)
	{
		int top = maxWinY - scrollDelta;
		if (top < minWinY)
			top = minWinY;
		FindOverlappingNodesInScreenRegion(top, maxWinY);
	}
	else if (scrollDelta < 0)
	{
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
		clearContext.surface->FillRect(clearContext, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, app.page.colourScheme.pageColour);
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
		clearContext.surface->FillRect(clearContext, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, app.page.colourScheme.pageColour);
	}

	Platform::input->ShowMouse();
}

bool PageRenderer::IsRenderableNode(Node* node)
{
	if (node->size.IsZero())
		return false;

	switch (node->type)
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

int PageRenderer::GetDrawOffsetY()
{
	Rect& windowRect = app.ui.windowRect;
	return windowRect.y - app.ui.GetScrollPositionY();
}

void PageRenderer::FindOverlappingNodesInScreenRegion(int top, int bottom)
{
	int drawOffsetY = GetDrawOffsetY();

	if (!lastCompleteNode)
		return;

	for(Node* node = app.page.GetRootNode(); node; node = node->GetNextInTree())
	{
		if(IsRenderableNode(node))
		{
			int nodeTop = node->anchor.y + drawOffsetY;
			int nodeBottom = nodeTop + node->size.y;

			if (nodeTop < top)
				nodeTop = top;
			if (nodeBottom > bottom)
				nodeBottom = bottom;

			if(nodeBottom - nodeTop > 0)
			{
				AddToQueue(node, nodeTop, nodeBottom);
			}
		}

		if (node == lastCompleteNode)
			break;
	}
}

void PageRenderer::Reset()
{
	renderQueue.Reset();
	lastCompleteNode = nullptr;
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
#ifdef _WIN32
	static int highWaterMark = 0;
	if (renderQueue.Size() > highWaterMark)
	{
		highWaterMark = renderQueue.Size();
		printf("High water mark: %d\n", highWaterMark);
	}

	//if (rand() % 256)
	//	return;
#endif

	int itemsToRender = 5;

	DrawContext itemContext;
	InitContext(itemContext);
	itemContext.drawOffsetY = GetDrawOffsetY();

	while(renderQueue.Size() && itemsToRender)
	{
		itemsToRender--;

		RenderQueue::Item* item = &renderQueue.items[renderQueue.head];
		Node* toRender = item->node;
		bool finishedRendering = true;

		Platform::input->HideMouse();
		itemContext.clipTop = item->upperClip;
		itemContext.clipBottom = item->lowerClip;
		
		if (toRender->type == Node::Image)
		{
			// Render images bit by bit
			const int imageLinesToRenderPerUpdate = 8;

			ImageNode::Data* imageData = static_cast<ImageNode::Data*>(toRender->data);
			if (imageData->state == ImageNode::FinishedDownloadingContent && imageData->image.lines.IsAllocated())
			{
				if (itemContext.clipBottom > itemContext.clipTop + imageLinesToRenderPerUpdate)
				{
					itemContext.clipBottom = itemContext.clipTop + imageLinesToRenderPerUpdate;
					item->upperClip = itemContext.clipBottom;
					finishedRendering = false;
				}
			}
		}

		toRender->Handler().Draw(itemContext, toRender);

		Platform::input->ShowMouse();

		if (finishedRendering)
		{
			renderQueue.Dequeue();
			itemsToRender = 0;
		}
	}
}	

void PageRenderer::AddToQueue(Node* node, int upperClip, int lowerClip)
{
	if (lowerClip <= upperClip)
	{
		return;
	}

	int insertPosition = renderQueue.tail;

	// Check if in queue already
	for (int i = renderQueue.head; i < renderQueue.tail; i++)
	{
		RenderQueue::Item* item = &renderQueue.items[i];
		if (item->node == node)
		{
			// Already in queue
			if ((upperClip <= item->upperClip && lowerClip >= item->upperClip)
				|| (lowerClip >= item->lowerClip && upperClip <= item->lowerClip))
			{
				// Grow clip region
				if (upperClip < item->upperClip)
				{
					item->upperClip = upperClip;
				}
				if (lowerClip > item->lowerClip)
				{
					item->lowerClip = lowerClip;
				}
				return;
			}
		}
		else if (item->node->IsChildOf(node))
		{
			if (insertPosition > i)
			{
				insertPosition = i;
			}
		}
	}

	if (renderQueue.Size() < MAX_RENDER_QUEUE_SIZE)
	{
		if (renderQueue.tail == MAX_RENDER_QUEUE_SIZE)
		{
			memcpy(renderQueue.items, renderQueue.items + renderQueue.head, renderQueue.Size() * sizeof(RenderQueue::Item));
			renderQueue.tail -= renderQueue.head;
			insertPosition -= renderQueue.head;
			renderQueue.head = 0;
		}

		if (insertPosition != renderQueue.tail)
		{
			for (int i = renderQueue.tail; i > insertPosition; --i)
			{
				renderQueue.items[i] = renderQueue.items[i - 1];
			}
		}

		RenderQueue::Item* item = &renderQueue.items[insertPosition];
		item->node = node;
		item->upperClip = upperClip;
		item->lowerClip = lowerClip;
		renderQueue.tail++;
	}
	else
	{
		//printf("Error! Out of space in render queue!\n");
	}
}

bool PageRenderer::IsInRenderQueue(Node* node)
{
	if (!node)
	{
		return false;
	}
	for (int i = renderQueue.head; i < renderQueue.tail; i++)
	{
		RenderQueue::Item* item = &renderQueue.items[i];
		if (item->node == node)
		{
			// Already in queue
			return true;
		}
	}
	return false;
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
	int drawOffsetY = GetDrawOffsetY();
	Node* startNode = lastCompleteNode ? lastCompleteNode->GetNextInTree() : app.page.GetRootNode();
	lastCompleteNode = node;

	int minWinY = windowRect.y;
	int maxWinY = windowRect.y + windowRect.height;

	bool expandedPage = false;

	for (Node* node = startNode; node; node = node->GetNextInTree())
	{
		if (IsRenderableNode(node))
		{
			int nodeTop = node->anchor.y + drawOffsetY;
			int nodeBottom = nodeTop + node->size.y;

			if (node->anchor.y + node->size.y > visiblePageHeight)
			{
				visiblePageHeight = node->anchor.y + node->size.y;
				expandedPage = true;
			}

			if (nodeTop < minWinY)
				nodeTop = minWinY;
			if (nodeBottom > maxWinY)
				nodeBottom = maxWinY;
			if (nodeBottom > nodeTop)
			{
				AddToQueue(node, nodeTop, nodeBottom);
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
	int drawOffsetY = GetDrawOffsetY();
	int minWinY = windowRect.y;
	int maxWinY = windowRect.y + windowRect.height;

	int nodeTop = dirtyNode->anchor.y + drawOffsetY;
	int nodeBottom = nodeTop + dirtyNode->size.y;
	bool outsideOfWindow = (nodeTop > maxWinY) || (nodeBottom < minWinY);

	if (!outsideOfWindow)
	{
		if (nodeTop < minWinY)
			nodeTop = minWinY;
		if (nodeBottom > maxWinY)
			nodeBottom = maxWinY;
		AddToQueue(dirtyNode, nodeTop, nodeBottom);

		Platform::input->HideMouse();

		Rect& windowRect = app.ui.windowRect;

		DrawContext clearContext;
		InitContext(clearContext);
		clearContext.clipBottom = windowRect.y + windowRect.height;
		clearContext.drawOffsetY = windowRect.y - app.ui.GetScrollPositionY();
		clearContext.surface->FillRect(clearContext, dirtyNode->anchor.x, dirtyNode->anchor.y, dirtyNode->size.x, dirtyNode->size.y, app.page.colourScheme.pageColour);

		Platform::input->ShowMouse();
	}
}

void PageRenderer::MarkPageLayoutComplete()
{
	Node* node = lastCompleteNode;

	if (!lastCompleteNode)
	{
		node = app.page.GetRootNode();
	}

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

void PageRenderer::InvertNode(Node* node)
{
	Platform::input->HideMouse();
	Rect& windowRect = app.ui.windowRect;
	DrawContext invertContext;

	InitContext(invertContext);
	invertContext.clipBottom = windowRect.y + windowRect.height;

	//if (invertContext.clipTop < upperContext.clipBottom)
	//{
	//	invertContext.clipTop = upperContext.clipBottom;
	//}
	//if (invertContext.clipBottom > lowerContext.clipTop)
	//{
	//	invertContext.clipBottom = lowerContext.clipTop;
	//}

	invertContext.drawOffsetY = windowRect.y - app.ui.GetScrollPositionY();
	invertContext.surface->InvertRect(invertContext, node->anchor.x, node->anchor.y, node->size.x, node->size.y);

	Platform::input->ShowMouse();
}

