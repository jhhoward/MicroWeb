#include "Layout.h"
#include "Page.h"
#include "Platform.h"
#include "App.h"
#include "Render.h"

Layout::Layout(Page& inPage)
	: page(inPage)
{
}

void Layout::Reset()
{
	paramStackSize = 0;
	cursorStackSize = 0;
	currentLineHeight = 0;
	lineStartNode = nullptr;
	lastNodeContext = nullptr;
	Cursor().Clear();

	LayoutParams& params = GetParams();
	params.marginLeft = 0;
	params.marginRight = page.GetPageWidth();
}

void Layout::PushCursor()
{
	if (cursorStackSize < MAX_CURSOR_STACK_SIZE - 1)
	{
		cursorStack[cursorStackSize + 1] = cursorStack[cursorStackSize];
		cursorStackSize++;
	}
	else
	{
		// TODO error
		exit(1);
	}
}

void Layout::PopCursor()
{
	if (cursorStackSize > 0)
	{
		cursorStackSize--;
	}
	else
	{
		// TODO: error
		exit(1);
	}
}

void Layout::PushLayout()
{
	if (paramStackSize < MAX_LAYOUT_PARAMS_STACK_SIZE - 1)
	{
		paramStack[paramStackSize + 1] = paramStack[paramStackSize];
		paramStackSize++;
	}
	else
	{
		// TODO error
		exit(1);
	}
}

void Layout::PopLayout()
{
	if (paramStackSize > 0)
	{
		paramStackSize--;
	}
	else
	{
		// TODO error
		exit(1);
	}
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
	//	TranslateNodes(lineStartNode, lastNodeContext, shift, 0);
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

		//if (node->parent)
		//{
		//	node->parent->OnChildLayoutChanged();
		//}

		if (node == end)
		{
			break;
		}
	}
}

/*
void Layout::TranslateNodes(Node* node, int deltaX, int deltaY, bool visitSiblings)
{
	while (node)
	{
		node->anchor.x += deltaX;
		node->anchor.y += deltaY;

		TranslateNodes(node->firstChild, deltaX, deltaY);

		if (visitSiblings)
		{
			if (node->next)
			{
				node = node->next;
			}
			else 
			{
				while(1)
				{
					if (!node->parent)
					{
						return;
					}
					if (!node->parent->next)
					{
						node = node->parent;
					}
					else
					{
						node = node->parent->next;
						break;
					}
				}
			}
		}
		else
		{
			node = node->next;
		}
	}
}
*/

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

#if 0
	//for (Node* node = page.GetRootNode(); node; node = node->GetNextInTree())
	//{
	//	node->Handler().GenerateLayout(*this, node);
	//}
	
	Node* node = page.GetRootNode();
	bool checkChildren = true;

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
//				node->EncapsulateChildren();
			}
			checkChildren = false;
			continue;
		}

		if (node)
		{
			node->Handler().BeginLayoutContext(*this, node);
			node->Handler().GenerateLayout(*this, node);
			/*if (node->parent)
			{
				node->parent->OnChildLayoutChanged();
			}*/
		}
	}
#endif

	// FIXME
	page.GetApp().ui.ScrollRelative(0);

#ifdef _WIN32
	page.DebugDumpNodeGraph(page.GetRootNode());
#endif
}
