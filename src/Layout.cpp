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
	cursor.Clear();
	paramStackSize = 0;
	currentLineHeight = 0;
	lineStartNode = nullptr;

	LayoutParams& params = GetParams();
	params.marginLeft = 0;
	params.marginRight = page.GetPageWidth();
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
	}
}

void Layout::BreakNewLine()
{
	// Recenter items if required
	if (lineStartNode && lineStartNode->style.alignment == ElementAlignment::Center)
	{
		int shift = AvailableWidth() / 2;
		TranslateNodes(lineStartNode, shift, 0, true);
	}

	if (lineStartNode && lineStartNode->parent)
	{
		lineStartNode->parent->OnChildLayoutChanged();
	}

	cursor.x = GetParams().marginLeft;
	cursor.y += currentLineHeight;
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

	if (lineHeight > currentLineHeight)
	{
		// Line height has increased so move everything down accordingly
		int deltaY = lineHeight - currentLineHeight;
		TranslateNodes(lineStartNode, 0, deltaY, true);
		currentLineHeight = lineHeight;
	}

	cursor.x += width;
}

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

void Layout::OnNodeEmitted(Node* node)
{
	if (!lineStartNode)
	{
		lineStartNode = node;
	}

	node->Handler().GenerateLayout(*this, node);

	if (node->parent)
	{
		node->parent->OnChildLayoutChanged();
	}
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
	if (cursor.x < params.marginLeft)
	{
		cursor.x = params.marginLeft;
	}
}

void Layout::PadVertical(int down)
{
	cursor.x = GetParams().marginLeft;
	cursor.y += down;

}

void Layout::RecalculateLayout()
{
	Reset();
	
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
			node = node->next;
			checkChildren = true;
		}
		else
		{
			node = node->parent;
			if (node)
			{
				node->EncapsulateChildren();
			}
			checkChildren = false;
			continue;
		}

		if (node)
		{
			node->Handler().GenerateLayout(*this, node);
			/*if (node->parent)
			{
				node->parent->OnChildLayoutChanged();
			}*/
		}
	}

	// FIXME
	page.GetApp().ui.ScrollRelative(0);

#ifdef _WIN32
	page.DebugDumpNodeGraph(page.GetRootNode());
#endif
}
