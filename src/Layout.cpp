#include "Layout.h"
#include "Page.h"
#include "Platform.h"
#include "App.h"
#include "Render.h"
#include "Nodes/ImgNode.h"

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
	currentNodeToProcess = nullptr;
	Cursor().Clear();
	tableDepth = 0;
	isFinished = false;

	LayoutParams& params = GetParams();
	params.marginLeft = 0;
	params.marginRight = page.GetApp().ui.windowRect.width;
}

void Layout::Update()
{
	while (currentNodeToProcess && currentNodeToProcess != lastNodeToProcess)
	{
		if (currentNodeToProcess->type == Node::Image && App::Get().config.loadImages)
		{
			ImageNode::Data* imageData = static_cast<ImageNode::Data*>(currentNodeToProcess->data);
			if (!imageData->HasDimensions())
			{
				// Waiting to first determine image size
				if (!App::Get().pageContentLoadTask.IsBusy())
				{
					App::Get().LoadImageNodeContent(currentNodeToProcess);
				}
				return;
			}
		}

		currentNodeToProcess->Handler().BeginLayoutContext(*this, currentNodeToProcess);
		currentNodeToProcess->Handler().GenerateLayout(*this, currentNodeToProcess);

		if (currentNodeToProcess->firstChild)
		{
			currentNodeToProcess = currentNodeToProcess->firstChild;
		}
		else
		{
			currentNodeToProcess->Handler().EndLayoutContext(*this, currentNodeToProcess);

			if (!tableDepth && !lineStartNode)
			{
				page.GetApp().pageRenderer.MarkNodeLayoutComplete(currentNodeToProcess);
			}

			if (currentNodeToProcess->next)
			{
				currentNodeToProcess = currentNodeToProcess->next;
			}
			else if(currentNodeToProcess->parent)
			{
				while (currentNodeToProcess)
				{
					currentNodeToProcess = currentNodeToProcess->parent;

					if (currentNodeToProcess)
					{
						currentNodeToProcess->Handler().EndLayoutContext(*this, currentNodeToProcess);

						if (currentNodeToProcess->next)
						{
							currentNodeToProcess = currentNodeToProcess->next;
							break;
						}
					}
				}
			}
		}

	}

	if (!isFinished && App::Get().parser.IsFinished() && !currentNodeToProcess)
	{
		if (!MemoryManager::pageAllocator.GetError())
		{
			page.GetApp().pageRenderer.MarkPageLayoutComplete();
		}

		// Layout has finished so now we can load image content
		App::Get().LoadImageNodeContent(page.GetRootNode());
		page.GetApp().ui.SetStatusMessage("Loading images...", StatusBarNode::GeneralStatus);
		isFinished = true;
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
	
	if (lineStartNode)
	{
		if (lineStartNode->style.alignment == ElementAlignment::Center)
		{
			int shift = AvailableWidth() / 2;
			TranslateNodes(lineStartNode, lastNodeContext, shift, 0);
		}
		else if (lineStartNode->style.alignment == ElementAlignment::Right)
		{
			int shift = AvailableWidth();
			TranslateNodes(lineStartNode, lastNodeContext, shift, 0);
		}
	}

	if (lastNodeContext && !tableDepth)
	{
		//page.GetApp().pageRenderer.MarkNodeLayoutComplete(lastNodeContext);
	}

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
	if (!currentNodeToProcess)
	{
		currentNodeToProcess = page.GetRootNode();
	}

	lastNodeToProcess = node;

//	if (!lineStartNode)
//	{
//		lineStartNode = node;
//	}
//
//	node->Handler().GenerateLayout(*this, node);
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

void Layout::MarkParsingComplete()
{
	lastNodeToProcess = nullptr;
}
