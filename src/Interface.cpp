//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "Platform.h"
#include "Interface.h"
#include "App.h"
#include "KeyCodes.h"
#include "Nodes/Section.h"
#include "Nodes/Button.h"
#include "Nodes/Field.h"
#include "Nodes/LinkNode.h"
#include "Nodes/Text.h"
#include "Nodes/Status.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Scroll.h"
#include "Draw/Surface.h"
#include "DataPack.h"
#include "Event.h"
#include "Memory/Memory.h"

AppInterface::AppInterface(App& inApp) : app(inApp)
{
	oldMouseX = -1;
	oldMouseY = -1;
	oldButtons = 0;

	hoverNode = nullptr;
	focusedNode = nullptr;
	jumpTagName = nullptr;
	jumpNode = nullptr;
}

void AppInterface::Init()
{
	GenerateInterfaceNodes();

	SetTitle("MicroWeb");

	DrawContext context(Platform::video->drawSurface, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight);
	DrawInterfaceNodes(context);
}

void AppInterface::Reset()
{
	scrollPositionY = 0;
	oldPageHeight = 0;

	if (focusedNode && !IsInterfaceNode(focusedNode))
	{
		focusedNode = nullptr;
	}
	focusedNode = nullptr;
	if (hoverNode && !IsInterfaceNode(hoverNode))
	{
		hoverNode = nullptr;
	}
	jumpTagName = nullptr;
	jumpNode = nullptr;

	ClearStatusMessage(StatusBarNode::HoverStatus);
	ClearStatusMessage(StatusBarNode::GeneralStatus);
	UpdatePageScrollBar();
}

void AppInterface::Update()
{
	int buttons, mouseX, mouseY;
	Platform::input->GetMouseStatus(buttons, mouseX, mouseY);

	Node* oldHoverNode = hoverNode;

	if (hoverNode && !IsOverNode(hoverNode, mouseX, mouseY))
	{
		hoverNode = PickNode(mouseX, mouseY);
	}
	else if (!hoverNode && (mouseX != oldMouseX || mouseY != oldMouseY))
	{
		hoverNode = PickNode(mouseX, mouseY);
	}

	int clickX, clickY;

	if (Platform::input->GetMouseButtonPress(clickX, clickY))
	{
		hoverNode = PickNode(clickX, clickY);
		HandleClick(mouseX, mouseY);
	}

	if ((buttons & 1) && (oldButtons & 1) && (mouseX != oldMouseX || mouseY != oldMouseY))
	{
		HandleDrag(mouseX, mouseY);
	}

	if (Platform::input->GetMouseButtonRelease(clickX, clickY))
	{
		HandleRelease(clickX, clickY);
	}

	oldMouseX = mouseX;
	oldMouseY = mouseY;
	oldButtons = buttons;

	if (hoverNode != oldHoverNode)
	{
		bool hasHoverStatusMessage = false;

		if (hoverNode)
		{
			switch (hoverNode->type)
			{
			case Node::Link:
				Platform::input->SetMouseCursor(MouseCursor::Hand);
				{
					LinkNode::Data* linkData = static_cast<LinkNode::Data*>(hoverNode->data);
					if (linkData && linkData->url)
					{
						SetStatusMessage(URL::GenerateFromRelative(app.page.pageURL.url, linkData->url).url, StatusBarNode::HoverStatus);
						hasHoverStatusMessage = true;
					}
				}
				break;
			case Node::TextField:
				Platform::input->SetMouseCursor(MouseCursor::TextSelect);
				break;
			case Node::Image:
				{
					ImageNode::Data* imageData = static_cast<ImageNode::Data*>(hoverNode->data);
					if (imageData && imageData->altText)
					{
						SetStatusMessage(imageData->altText, StatusBarNode::HoverStatus);
						hasHoverStatusMessage = true;
					}
				}
				break;
			default:
				Platform::input->SetMouseCursor(MouseCursor::Pointer);
				break;
			}
		}
		else
		{
			Platform::input->SetMouseCursor(MouseCursor::Pointer);
		}

		if (!hasHoverStatusMessage)
		{
			ClearStatusMessage(StatusBarNode::HoverStatus);
		}

		if(0)
		{
			// For debugging picking
			Platform::input->HideMouse();
			if (hoverNode)
			{
				DrawContext context;
				app.pageRenderer.GenerateDrawContext(context, hoverNode);
				context.surface->InvertRect(context, hoverNode->anchor.x, hoverNode->anchor.y, hoverNode->size.x, hoverNode->size.y);
			}
			if (oldHoverNode)
			{
				DrawContext context;
				app.pageRenderer.GenerateDrawContext(context, oldHoverNode);
				context.surface->InvertRect(context, oldHoverNode->anchor.x, oldHoverNode->anchor.y, oldHoverNode->size.x, oldHoverNode->size.y);
			}
			Platform::input->ShowMouse();
		}

	}
	

	InputButtonCode keyPress;
	int scrollDelta = 0;

	while (keyPress = Platform::input->GetKeyPress())
	{
		if (focusedNode && focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::KeyPress, keyPress)))
		{
			continue;
		}

		switch (keyPress)
		{
//		case KEYCODE_MOUSE_LEFT:
//			HandleClick();
//			break;
		case KEYCODE_ESCAPE:
			app.Close();
			break;
		case KEYCODE_ARROW_UP:
			scrollDelta -= 8;
			break;
		case KEYCODE_ARROW_DOWN:
			scrollDelta += 8;
			break;
		case KEYCODE_PAGE_UP:
			scrollDelta -= (windowRect.height - 24);
			break;
		case KEYCODE_PAGE_DOWN:
			scrollDelta += (windowRect.height - 24);
			break;
		case KEYCODE_HOME:
			ScrollAbsolute(0);
			break;
		case KEYCODE_END:
			{
				int end = app.pageRenderer.GetVisiblePageHeight() - windowRect.height;
				if (end > 0)
				{
					ScrollAbsolute(end);
				}
			}
			break;
		case KEYCODE_BACKSPACE:
			app.PreviousPage();
			break;
		case KEYCODE_F2:
		{
			Platform::video->InvertVideoOutput();
		}
			break;
		case KEYCODE_CTRL_L:
		case KEYCODE_F6:
			FocusNode(addressBarNode);
			break;
		case KEYCODE_F5:
			app.ReloadPage();
			break;
		case KEYCODE_F3:
			ToggleStatusAndTitleBar();
			break;

		case KEYCODE_TAB:
			CycleNodes(1);
			break;
		case KEYCODE_SHIFT_TAB:
			CycleNodes(-1);
			break;

		case 'm':
		{
			char tempMessage[100];
			MemoryManager::GenerateMemoryReport(tempMessage);
			SetStatusMessage(tempMessage, StatusBarNode::GeneralStatus);
		}
			break;

		case 'n':
		{
#ifdef _WIN32
			app.page.DebugDumpNodeGraph();
#endif
		}

		default:
//			printf("%x\n", keyPress);
			break;
		}
	}

	if (scrollDelta)
	{
		ScrollRelative(scrollDelta);
	}

	if (jumpNode)
	{
		Node* node = App::Get().ui.jumpNode;
		while (node && node->size.IsZero())
		{
			node = node->GetNextInTree();
		}

		if (node)
		{
			int jumpPosition = node->anchor.y;

			if (app.page.layout.IsFinished() || jumpPosition + windowRect.height < app.pageRenderer.GetVisiblePageHeight())
			{
				ScrollAbsolute(jumpPosition);
				jumpNode = nullptr;
			}			
		}
	}
}

bool AppInterface::IsOverNode(Node* node, int x, int y)
{
	if (!node)
	{
		return false;
	}
	if (IsInterfaceNode(node))
	{
		return node->IsPointInsideChildren(x, y);
	}
	else
	{
		if (x >= windowRect.x && y >= windowRect.y && x < windowRect.x + windowRect.width && y < windowRect.y + windowRect.height)
		{
			x -= windowRect.x;
			y -= windowRect.y - scrollPositionY;
			return node->IsPointInsideChildren(x, y);
		}
		return false;
	}
}

Node* AppInterface::PickNode(int x, int y)
{
	Node* interfaceNode = rootInterfaceNode->Handler().Pick(rootInterfaceNode, x, y);

	if (interfaceNode)
	{
		return interfaceNode;
	}

	if (x >= windowRect.x && y >= windowRect.y && x < windowRect.x + windowRect.width && y < windowRect.y + windowRect.height)
	{
		int pageX = x - windowRect.x;
		int pageY = y - windowRect.y + scrollPositionY;

		Node* pageRootNode = app.page.GetRootNode();
		return pageRootNode->Handler().Pick(pageRootNode, pageX, pageY);
	}

	return nullptr;
}

void AppInterface::HandleClick(int mouseX, int mouseY)
{
	if (hoverNode)
	{
		hoverNode->Handler().HandleEvent(hoverNode, Event(app, Event::MouseClick, mouseX, mouseY));
	}
	else if (focusedNode)
	{
		FocusNode(nullptr);
	}
}

void AppInterface::HandleDrag(int mouseX, int mouseY)
{
	if (focusedNode)
	{
		focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::MouseDrag, mouseX, mouseY));
	}
}

void AppInterface::HandleRelease(int mouseX, int mouseY)
{
	if (focusedNode)
	{
		focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::MouseRelease, mouseX, mouseY));
	}

}

void AppInterface::UpdateAddressBar(const URL& url)
{
	addressBarURL = url;
	addressBarNode->Redraw();

	jumpTagName = strstr(url.url, "#");
}

void AppInterface::UpdatePageScrollBar()
{
	ScrollBarNode::Data* data = static_cast<ScrollBarNode::Data*>(scrollBarNode->data);
	int maxScrollHeight = app.pageRenderer.GetVisiblePageHeight() - app.ui.windowRect.height;
	if (maxScrollHeight < 0)
		maxScrollHeight = 0;
	data->scrollPosition = scrollPositionY;
	data->maxScroll = maxScrollHeight;
	scrollBarNode->Redraw();
}

void AppInterface::GenerateInterfaceNodes()
{
	Allocator& allocator = MemoryManager::interfaceAllocator;
	ElementStyle rootInterfaceStyle;
	rootInterfaceStyle.alignment = ElementAlignment::Left;
	rootInterfaceStyle.fontSize = 1;
	rootInterfaceStyle.fontStyle = FontStyle::Regular;
	rootInterfaceStyle.fontColour = Platform::video->colourScheme.textColour;

	rootInterfaceNode = SectionElement::Construct(allocator, SectionElement::Interface);
	rootInterfaceNode->SetStyle(rootInterfaceStyle);

	Font* interfaceFont = Assets.GetFont(1, FontStyle::Regular);
	Font* smallInterfaceFont = Assets.GetFont(0, FontStyle::Regular);

	{
		titleBuffer[0] = '\0';
		TextElement::Data* titleNodeData = allocator.Alloc<TextElement::Data>(MemBlockHandle (titleBuffer));
		titleNode = allocator.Alloc<Node>(Node::Text, titleNodeData);
		titleNode->anchor.Clear();
		titleNode->size.x = Platform::video->screenWidth;
		titleNode->size.y = interfaceFont->glyphHeight;
		titleNode->styleHandle = rootInterfaceNode->styleHandle;
		rootInterfaceNode->AddChild(titleNode);
	}

	backButtonNode = ButtonNode::Construct(allocator, " < ", OnBackButtonPressed);
	backButtonNode->styleHandle = rootInterfaceNode->styleHandle;
	backButtonNode->size = ButtonNode::CalculateSize(backButtonNode);
	backButtonNode->anchor.x = 1;
	backButtonNode->anchor.y = titleNode->size.y;
	rootInterfaceNode->AddChild(backButtonNode);

	forwardButtonNode = ButtonNode::Construct(allocator, " > ", OnForwardButtonPressed);
	forwardButtonNode->styleHandle = rootInterfaceNode->styleHandle;
	forwardButtonNode->size = ButtonNode::CalculateSize(forwardButtonNode);
	forwardButtonNode->anchor.x = backButtonNode->anchor.x + backButtonNode->size.x + 2;
	forwardButtonNode->anchor.y = titleNode->size.y;
	rootInterfaceNode->AddChild(forwardButtonNode);

	addressBarNode = TextFieldNode::Construct(allocator, addressBarURL.url, MAX_URL_LENGTH - 1, OnAddressBarSubmit);
	addressBarNode->styleHandle = rootInterfaceNode->styleHandle;
	addressBarNode->anchor.x = forwardButtonNode->anchor.x + forwardButtonNode->size.x + 2;
	addressBarNode->anchor.y = titleNode->size.y;
	addressBarNode->size.x = Platform::video->screenWidth - addressBarNode->anchor.x - 1;
	addressBarNode->size.y = backButtonNode->size.y;
	rootInterfaceNode->AddChild(addressBarNode);

	statusBarNode = StatusBarNode::Construct(allocator);
	statusBarNode->size.x = Platform::video->screenWidth;
	statusBarNode->size.y = smallInterfaceFont->glyphHeight + 2;
	statusBarNode->anchor.x = 0;
	statusBarNode->anchor.y = Platform::video->screenHeight - statusBarNode->size.y;
	rootInterfaceNode->AddChild(statusBarNode);

	ElementStyle statusBarStyle = rootInterfaceStyle;
	statusBarStyle.fontSize = 0;
	statusBarNode->SetStyle(statusBarStyle);

	scrollBarNode = ScrollBarNode::Construct(allocator, scrollPositionY, app.page.pageHeight, OnScrollBarMoved);
	scrollBarNode->styleHandle = rootInterfaceNode->styleHandle;
	scrollBarNode->anchor.y = backButtonNode->anchor.y + backButtonNode->size.y + 2;
	scrollBarNode->size.x = 16;
	scrollBarNode->size.y = Platform::video->screenHeight - scrollBarNode->anchor.y - statusBarNode->size.y;
	scrollBarNode->anchor.x = Platform::video->screenWidth - scrollBarNode->size.x;
	rootInterfaceNode->AddChild(scrollBarNode);

	windowRect.x = 0;
	windowRect.y = backButtonNode->anchor.y + backButtonNode->size.y + 2;
	windowRect.width = Platform::video->screenWidth - scrollBarNode->size.x;
	windowRect.height = Platform::video->screenHeight - windowRect.y - statusBarNode->size.y;

	pageHeightForDimensionScaling = windowRect.height;

	StylePool::Get().MarkInterfaceStylesComplete();
}

void AppInterface::DrawInterfaceNodes(DrawContext& context)
{
	Platform::input->HideMouse();
	Platform::video->drawSurface->Clear();

	app.pageRenderer.DrawAll(context, rootInterfaceNode);

	uint8_t dividerColour = Platform::video->colourScheme.textColour;
	context.surface->HLine(context, 0, windowRect.y - 1, Platform::video->screenWidth, dividerColour);
	Platform::input->ShowMouse();
}

void AppInterface::SetTitle(const char* title)
{
	strncpy(titleBuffer, title, MAX_TITLE_LENGTH);
	titleBuffer[MAX_TITLE_LENGTH - 1] = '\0';
	Font* font = titleNode->GetStyleFont();
	int titleWidth = font->CalculateWidth(titleBuffer, titleNode->GetStyle().fontStyle);
	titleNode->anchor.x = Platform::video->screenWidth / 2 - titleWidth / 2;
	if (titleNode->anchor.x < 0)
	{
		titleNode->anchor.x = 0;
	}

	DrawContext context(Platform::video->drawSurface, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight);
	Platform::input->HideMouse();
	uint8_t fillColour = Platform::video->colourScheme.pageColour;
	context.surface->FillRect(context, 0, titleNode->anchor.y, Platform::video->screenWidth, titleNode->size.y, fillColour);
	titleNode->Handler().Draw(context, titleNode);
	Platform::input->ShowMouse();
}

bool AppInterface::IsInterfaceNode(Node* node)
{
	return node == rootInterfaceNode || (node && node->parent == rootInterfaceNode);
}

void AppInterface::FocusNode(Node* node)
{
	if (node != focusedNode)
	{
		if (focusedNode)
		{
			focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::Unfocus));
		}

		focusedNode = node;

		if (focusedNode)
		{
			focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::Focus));
		}
	}
}

void AppInterface::OnBackButtonPressed(Node* node)
{
	App::Get().PreviousPage();
}

void AppInterface::OnForwardButtonPressed(Node* node)
{
	App::Get().NextPage();
}

void AppInterface::OnAddressBarSubmit(Node* node)
{
	App& app = App::Get();
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	app.OpenURL(data->buffer);
	app.ui.FocusNode(nullptr);
}

void AppInterface::SetStatusMessage(const char* message, StatusBarNode::StatusType type)
{
	if (message)
	{
		StatusBarNode::SetStatus(statusBarNode, message, type);
	}
	else ClearStatusMessage(type);
}

void AppInterface::ClearStatusMessage(StatusBarNode::StatusType type)
{
	StatusBarNode::SetStatus(statusBarNode, "", type);
}

void AppInterface::ScrollRelative(int delta)
{
	int oldScrollPositionY = scrollPositionY;

	scrollPositionY += delta;
	if (scrollPositionY < 0)
		scrollPositionY = 0;

	int maxScrollY = app.pageRenderer.GetVisiblePageHeight() - windowRect.height;
	if (maxScrollY < 0)
		maxScrollY = 0;

	if (scrollPositionY > maxScrollY)
	{
		scrollPositionY = maxScrollY;
	}

	delta = scrollPositionY - oldScrollPositionY;

	UpdatePageScrollBar();

	app.pageRenderer.OnPageScroll(delta);
}

void AppInterface::ScrollAbsolute(int position)
{
	int delta = position - scrollPositionY;
	if (delta)
	{
		ScrollRelative(delta);
	}
}

void AppInterface::CycleNodes(int direction)
{
	Node* node = focusedNode;

	if (!node)
	{
		node = app.page.GetRootNode();
	}

	bool isFocusedNodeVisible = false;
	if (focusedNode)
	{
		Rect nodeRect;
		node->CalculateEncapsulatingRect(nodeRect);
		bool offPage = (nodeRect.y + nodeRect.height < scrollPositionY || nodeRect.y > scrollPositionY + windowRect.height);
		isFocusedNodeVisible = !offPage;
		if (!isFocusedNodeVisible)
			node = app.page.GetRootNode();
	}

	if (node)
	{
		if (!IsInterfaceNode(node))
		{
			while (node)
			{
				if (direction > 0)
				{
					node = node->GetNextInTree();
				}
				else
				{
					node = node->GetPreviousInTree();
				}

				if (node && node->Handler().CanPick(node))
				{
					Rect nodeRect;
					node->CalculateEncapsulatingRect(nodeRect);

					if (!isFocusedNodeVisible && (nodeRect.y + nodeRect.height < scrollPositionY || nodeRect.y > scrollPositionY + windowRect.height))
					{
						// Not visible on page and we haven't selected anything yet, try find something else to focus on
						continue;
					}

					if (nodeRect.y < scrollPositionY)
					{
						ScrollAbsolute(nodeRect.y);
					}
					else if (nodeRect.y + nodeRect.height > scrollPositionY + windowRect.height)
					{
						ScrollAbsolute(nodeRect.y + nodeRect.height - windowRect.height);
					}
					FocusNode(node);
					return;
				}
			}
		}
	}
}

void AppInterface::OnScrollBarMoved(Node* node)
{
	ScrollBarNode::Data* data = static_cast<ScrollBarNode::Data*>(node->data);
	AppInterface& ui = App::Get().ui;
	ui.ScrollRelative(data->scrollPosition - ui.scrollPositionY);
}

void AppInterface::ToggleStatusAndTitleBar()
{
	int upperShift = titleNode->size.y;
	int lowerShift = statusBarNode->size.y;


	if (titleNode->anchor.y < 0)
	{
		upperShift = -upperShift;
		lowerShift = -lowerShift;
	}

	titleNode->anchor.y -= upperShift;
	windowRect.y -= upperShift;
	windowRect.height += upperShift;
	backButtonNode->anchor.y -= upperShift;
	forwardButtonNode->anchor.y -= upperShift;
	addressBarNode->anchor.y -= upperShift;
	scrollBarNode->anchor.y -= upperShift;
	scrollBarNode->size.y += upperShift;
	statusBarNode->anchor.y += lowerShift;
	scrollBarNode->size.y += lowerShift;
	windowRect.height += lowerShift;

	Platform::input->HideMouse();
	DrawContext context(Platform::video->drawSurface, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight);
	context.surface->FillRect(context, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, Platform::video->colourScheme.pageColour);
	DrawInterfaceNodes(context);
	app.pageRenderer.RefreshAll();
	Platform::input->ShowMouse();
}
