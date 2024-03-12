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
#include "Nodes/Text.h"
#include "Nodes/Status.h"
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
}

void AppInterface::Init()
{
	GenerateInterfaceNodes();

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
}

void AppInterface::Update()
{
	if (app.page.GetPageHeight() != oldPageHeight)
	{
		oldPageHeight = app.page.GetPageHeight();
		UpdatePageScrollBar();
	}

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

	if ((buttons & 1) && !(oldButtons & 1))
	{
		HandleClick(mouseX, mouseY);
	}
	else if (!(buttons & 1) && (oldButtons & 1))
	{
		HandleRelease();
	}

	oldMouseX = mouseX;
	oldMouseY = mouseY;
	oldButtons = buttons;

	if (hoverNode != oldHoverNode)
	{
		if (hoverNode)
		{
			switch (hoverNode->type)
			{
			case Node::Link:
				Platform::input->SetMouseCursor(MouseCursor::Hand);
				// TODO show link URL
				break;
			case Node::TextField:
				Platform::input->SetMouseCursor(MouseCursor::TextSelect);
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
			//ScrollAbsolute(0);
			break;
		case KEYCODE_BACKSPACE:
			app.PreviousPage();
			break;
		case KEYCODE_F2:
		{
			// TODO-refactor
			//Platform::input->HideMouse();
			//Platform::video->InvertScreen();
			//Platform::input->ShowMouse();
		}
			break;
		case KEYCODE_CTRL_L:
		case KEYCODE_F6:
			//ActivateWidget(&addressBar);
			break;

		case KEYCODE_TAB:
			//CycleWidgets(1);
			break;
		case KEYCODE_SHIFT_TAB:
			//CycleWidgets(-1);
			break;

		case 'm':
		{
			char tempMessage[100];
			MemoryManager::GenerateMemoryReport(tempMessage);
			app.ui.SetStatusMessage(tempMessage);
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
		//app.renderer.Scroll(scrollDelta);
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
		x -= windowRect.x;
		y -= windowRect.y - scrollPositionY;
		return node->IsPointInsideChildren(x, y);
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
		FocusNode(hoverNode);
		focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::MouseClick));
	}
}

void AppInterface::HandleRelease()
{
	if (focusedNode)
	{
		focusedNode->Handler().HandleEvent(focusedNode, Event(app, Event::MouseRelease));
	}

}

void AppInterface::UpdateAddressBar(const URL& url)
{
	addressBarURL = url;
	addressBarNode->Redraw();
}

void AppInterface::UpdatePageScrollBar()
{
	ScrollBarNode::Data* data = static_cast<ScrollBarNode::Data*>(scrollBarNode->data);
	int maxScrollHeight = app.page.GetRootNode()->size.y - app.ui.windowRect.height;
	if (maxScrollHeight < 0)
		maxScrollHeight = 0;
	data->scrollPosition = scrollPositionY;
	data->maxScroll = maxScrollHeight;
	scrollBarNode->Redraw();
}

void AppInterface::GenerateInterfaceNodes()
{
	Allocator& allocator = MemoryManager::interfaceAllocator;
	rootInterfaceNode = SectionElement::Construct(allocator, SectionElement::Interface);
	rootInterfaceNode->style.alignment = ElementAlignment::Left;
	rootInterfaceNode->style.fontSize = 1;
	rootInterfaceNode->style.fontStyle = FontStyle::Regular;
	rootInterfaceNode->style.fontColour = Platform::video->colourScheme.textColour;

	Font* interfaceFont = Assets.GetFont(1, FontStyle::Regular);
	Font* smallInterfaceFont = Assets.GetFont(0, FontStyle::Regular);

	{
		titleBuffer[0] = '\0';
		TextElement::Data* titleNodeData = allocator.Alloc<TextElement::Data>(MemBlockHandle (titleBuffer));
		titleNode = allocator.Alloc<Node>(Node::Text, titleNodeData);
		titleNode->anchor.Clear();
		titleNode->size.x = Platform::video->screenWidth;
		titleNode->size.y = interfaceFont->glyphHeight;
		titleNode->style = rootInterfaceNode->style;
		rootInterfaceNode->AddChild(titleNode);
	}

	backButtonNode = ButtonNode::Construct(allocator, " < ", OnBackButtonPressed);
	backButtonNode->style = rootInterfaceNode->style;
	backButtonNode->size = ButtonNode::CalculateSize(backButtonNode);
	backButtonNode->anchor.x = 1;
	backButtonNode->anchor.y = titleNode->size.y;
	rootInterfaceNode->AddChild(backButtonNode);

	forwardButtonNode = ButtonNode::Construct(allocator, " > ", OnForwardButtonPressed);
	forwardButtonNode->style = rootInterfaceNode->style;
	forwardButtonNode->size = ButtonNode::CalculateSize(forwardButtonNode);
	forwardButtonNode->anchor.x = backButtonNode->anchor.x + backButtonNode->size.x + 2;
	forwardButtonNode->anchor.y = titleNode->size.y;
	rootInterfaceNode->AddChild(forwardButtonNode);

	addressBarNode = TextFieldNode::Construct(allocator, addressBarURL.url, MAX_URL_LENGTH - 1, OnAddressBarSubmit);
	addressBarNode->style = rootInterfaceNode->style;
	addressBarNode->anchor.x = forwardButtonNode->anchor.x + forwardButtonNode->size.x + 2;
	addressBarNode->anchor.y = titleNode->size.y;
	addressBarNode->size.x = Platform::video->screenWidth - addressBarNode->anchor.x - 1;
	addressBarNode->size.y = backButtonNode->size.y;
	rootInterfaceNode->AddChild(addressBarNode);

	statusBarNode = StatusBarNode::Construct(allocator);
	statusBarNode->style = rootInterfaceNode->style;
	statusBarNode->size.x = Platform::video->screenWidth;
	statusBarNode->size.y = smallInterfaceFont->glyphHeight + 2;
	statusBarNode->anchor.x = 0;
	statusBarNode->anchor.y = Platform::video->screenHeight - statusBarNode->size.y;
	rootInterfaceNode->AddChild(statusBarNode);
	statusBarNode->style.fontSize = 0;

	scrollBarNode = ScrollBarNode::Construct(allocator, scrollPositionY, app.page.pageHeight);
	scrollBarNode->style = rootInterfaceNode->style;
	scrollBarNode->anchor.y = backButtonNode->anchor.y + backButtonNode->size.y + 3;
	scrollBarNode->size.x = 16;
	scrollBarNode->size.y = Platform::video->screenHeight - scrollBarNode->anchor.y - statusBarNode->size.y;
	scrollBarNode->anchor.x = Platform::video->screenWidth - scrollBarNode->size.x;
	rootInterfaceNode->AddChild(scrollBarNode);

	rootInterfaceNode->EncapsulateChildren();

	windowRect.x = 0;
	windowRect.y = backButtonNode->anchor.y + backButtonNode->size.y + 3;
	windowRect.width = Platform::video->screenWidth - scrollBarNode->size.x;
	windowRect.height = Platform::video->screenHeight - windowRect.y - statusBarNode->size.y;
}

void AppInterface::DrawInterfaceNodes(DrawContext& context)
{
	app.pageRenderer.DrawAll(context, rootInterfaceNode);

	Platform::input->HideMouse();
	uint8_t dividerColour = 0;
	context.surface->HLine(context, 0, windowRect.y - 1, Platform::video->screenWidth, dividerColour);
	Platform::input->ShowMouse();
}

void AppInterface::SetTitle(const char* title)
{
	strncpy(titleBuffer, title, MAX_TITLE_LENGTH);
	Font* font = Assets.GetFont(titleNode->style.fontSize, titleNode->style.fontStyle);
	int titleWidth = font->CalculateWidth(titleBuffer, titleNode->style.fontStyle);
	titleNode->anchor.x = Platform::video->screenWidth / 2 - titleWidth / 2;
	if (titleNode->anchor.x < 0)
	{
		titleNode->anchor.x = 0;
	}

	DrawContext context(Platform::video->drawSurface, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight);
	Platform::input->HideMouse();
	uint8_t fillColour = Platform::video->colourScheme.pageColour;
	context.surface->FillRect(context, 0, 0, titleNode->size.x, titleNode->size.y, fillColour);
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
		focusedNode = node;
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
}

void AppInterface::SetStatusMessage(const char* message)
{
	StatusBarNode::Data* data = static_cast<StatusBarNode::Data*>(statusBarNode->data);
	if (strcmp(data->message, message))
	{
		strcpy(data->message, message);
		statusBarNode->Redraw();
	}
}

void AppInterface::ScrollRelative(int delta)
{
	int oldScrollPositionY = scrollPositionY;

	scrollPositionY += delta;
	if (scrollPositionY < 0)
		scrollPositionY = 0;

	int maxScrollY = app.page.GetRootNode()->size.y - app.ui.windowRect.height;
	if (maxScrollY > 0 && scrollPositionY > maxScrollY)
	{
		scrollPositionY = maxScrollY;
	}

	delta = scrollPositionY - oldScrollPositionY;

	UpdatePageScrollBar();

	//Platform::input->HideMouse();
	
	app.pageRenderer.OnPageScroll(delta);
	
	//DrawContext context;
	//app.pageRenderer.GenerateDrawContext(context, NULL);
	//context.drawOffsetY = 0;
	//context.surface->FillRect(context, 0, 0, Platform::video->screenWidth, Platform::video->screenHeight, Platform::video->colourScheme.pageColour);
	//app.pageRenderer.GenerateDrawContext(context, NULL);
	////app.pageRenderer.DrawAll(context, app.page.GetRootNode());

	Platform::input->ShowMouse();
}

void AppInterface::ScrollAbsolute(int position)
{

}
