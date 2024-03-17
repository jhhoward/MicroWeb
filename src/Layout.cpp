#include "Layout.h"
#include "Page.h"
#include "Platform.h"
#include "App.h"
#include "Render.h"

Layout::Layout(Page& inPage)
	: page(inPage), cursorStack(MemoryManager::pageAllocator), paramStack(MemoryManager::pageAllocator)
{
}

void Layout::Reset()
{
	paramStack.Reset();
	cursorStack.Reset();
	currentLineHeight = 0;
	lineStartNode = nullptr;
	lastNodeContext = nullptr;
	Cursor().Clear();
	tableDepth = 0;

	LayoutParams& params = GetParams();
	params.marginLeft = 0;
	params.marginRight = page.GetApp().ui.windowRect.width;
}

void Layout::BreakNewLine()
{
	// Recenter items if required
	//if (lineStartNode && lineStartNode->style.alignment == ElementAlignment::Center)
	//{
	//	int shift = AvailableWidth() / 2;
	//	TranslateNodes(lineStartNode, shift, 0, true);
	//}
	
	if (lineStartNode && lineStartNode->style.alignment == ElementAlignment::Center)
	{
		int shift = AvailableWidth() / 2;
		TranslateNodes(lineStartNode, lastNodeContext, shift, 0);
	}

	if (lastNodeContext && !tableDepth)
	{
		page.GetApp().pageRenderer.MarkNodeLayoutComplete(lastNodeContext);
	}

	//if (lineStartNode && lineStartNode->parent)
	//{
	//	lineStartNode->parent->OnChildLayoutChanged();
	//}

	Cursor().x = GetParams().marginLeft;
	Cursor().y += currentLineHeight;
	currentLineHeight = 0;
	lineStartNode = nullptr;
}

void Layout::ProgressCursor(Node* nodeContext, int width, int lineHeight)
{
	// Horrible hack to add padding / spacing between nodes. FIXME
	if (width)
	{
		width += 2;
	}

	if (!lineStartNode)
	{
		lineStartNode = nodeContext;
	}

	lastNodeContext = nodeContext;

	if (lineHeight > currentLineHeight)
	{
		// Line height has increased so move everything down accordingly
		int deltaY = lineHeight - currentLineHeight;
//		TranslateNodes(lineStartNode, 0, deltaY, true);
		TranslateNodes(lineStartNode, nodeContext, 0, deltaY);
		currentLineHeight = lineHeight;
	}

	Cursor().x += width;
}

void Layout::TranslateNodes(Node* start, Node* end, int deltaX, int deltaY)
{
	for (Node* node = start; node; node = node->GetNextInTree())
	{
		node->anchor.x += deltaX;
		node->anchor.y += deltaY;

		if (node == end)
		{
			break;
		}
	}
}

void Layout::OnNodeEmitted(Node* node)
{
	if (!lineStartNode)
	{
		lineStartNode = node;
	}

	node->Handler().GenerateLayout(*this, node);

	//if (node->parent)
	//{
	//	node->parent->OnChildLayoutChanged();
	//}
}

void Layout::PadHorizontal(int left, int right)
{
	LayoutParams& params = GetParams();
	if (params.marginLeft + left >= params.marginRight - right)
	{
		params.marginLeft = (params.marginLeft + left + params.marginRight - right) / 2;
		params.marginRight = params.marginLeft + 1;
	}
	else
	{
		params.marginLeft += left;
		params.marginRight -= right;
	}
	if (Cursor().x < params.marginLeft)
	{
		Cursor().x = params.marginLeft;
	}
}

void Layout::RestrictHorizontal(int maxWidth)
{
	LayoutParams& params = GetParams();
	int marginRight = Cursor().x + maxWidth;
	if (marginRight < params.marginRight)
	{
		params.marginRight = marginRight;
	}
}

void Layout::PadVertical(int down)
{
	Cursor().x = GetParams().marginLeft;
	Cursor().y += down;

}

void Layout::RecalculateLayoutForNode(Node* targetNode)
{
	targetNode->Handler().BeginLayoutContext(*this, targetNode);
	targetNode->Handler().GenerateLayout(*this, targetNode);

	for (Node* node = targetNode->firstChild; node; node = node->next)
	{
		RecalculateLayoutForNode(node);
	}

	targetNode->Handler().EndLayoutContext(*this, targetNode);

#if 0
	Node* node = targetNode;
	bool checkChildren = true;

	node->Handler().BeginLayoutContext(*this, node);
	node->Handler().GenerateLayout(*this, node);

	while (node)
	{
		if (checkChildren && node->firstChild)
		{
			node = node->firstChild;
		}
		else if (node->next)
		{
			if (checkChildren)
			{
				// ???
				node->Handler().EndLayoutContext(*this, node);
			}

			node = node->next;
			checkChildren = true;
		}
		else
		{
			node = node->parent;
			if (node)
			{
				node->Handler().EndLayoutContext(*this, node);
			}

			if (node == targetNode)
			{
				break;
			}
			checkChildren = false;
			continue;
		}

		if (node)
		{
			node->Handler().BeginLayoutContext(*this, node);
			node->Handler().GenerateLayout(*this, node);
		}
	}
#endif
}

void Layout::RecalculateLayout()
{
	Reset();

	Node* node = page.GetRootNode();
	RecalculateLayoutForNode(node);

	page.GetApp().pageRenderer.MarkPageLayoutComplete();
	page.GetApp().pageRenderer.RefreshAll();

#ifdef _WIN32
	//page.DebugDumpNodeGraph(page.GetRootNode());
#endif
}
