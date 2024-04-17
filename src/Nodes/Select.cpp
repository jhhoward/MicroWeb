#include <stdio.h>
#include "../Layout.h"
#include "../Memory/LinAlloc.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"
#include "../App.h"
#include "../KeyCodes.h"
#include "Select.h"

Node* SelectNode::Construct(Allocator& allocator, const char* name)
{
	SelectNode::Data* data = allocator.Alloc<SelectNode::Data>(allocator.AllocString(name));
	if (data)
	{
		return allocator.Alloc<Node>(Node::Select, data);
	}
	return nullptr;
}

void SelectNode::ApplyStyle(Node* node)
{
	ElementStyle style = node->GetStyle();
	style.fontStyle = FontStyle::Regular;
	style.fontSize = 1;
	node->SetStyle(style);
}


void SelectNode::Draw(DrawContext& context, Node* node)
{
	SelectNode::Data* data = static_cast<SelectNode::Data*>(node->data);

	Font* font = node->GetStyleFont();
	uint8_t textColour = Platform::video->colourScheme.textColour;
	uint8_t buttonOutlineColour = Platform::video->colourScheme.textColour;
	uint8_t clearColour = Platform::video->colourScheme.pageColour;
	context.surface->FillRect(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, node->size.y - 2, clearColour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y, node->size.x - 2, buttonOutlineColour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 1, node->size.x - 2, buttonOutlineColour);
	context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);

	if (data->selected && data->selected->text)
	{
		context.surface->DrawString(context, font, data->selected->text, node->anchor.x + 3, node->anchor.y + 2, textColour, node->GetStyle().fontStyle);
	}
	context.surface->BlitImage(context, Assets.downIcon, node->anchor.x + node->size.x - Assets.downIcon->width - 2, node->anchor.y + (node->size.y - Assets.downIcon->height) / 2);

	if (App::Get().ui.GetFocusedNode() == node)
	{
		if (node == dropDownMenu.activeNode)
		{
			DrawDropDownMenu();
		}
		else
		{
			context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, buttonOutlineColour);
			context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 2, node->size.x - 2, buttonOutlineColour);
			context.surface->VLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
			context.surface->VLine(context, node->anchor.x + node->size.x - 2, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
		}
	}
}

void SelectNode::DrawHighlight(Node* node, uint8_t colour)
{
	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);

	Platform::input->HideMouse();
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, colour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 2, node->size.x - 2, colour);
	context.surface->VLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.y - 2, colour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 2, node->anchor.y + 1, node->size.y - 2, colour);
	Platform::input->ShowMouse();
}


void SelectNode::EndLayoutContext(Layout& layout, Node* node)
{
	SelectNode::Data* data = static_cast<SelectNode::Data*>(node->data);

	Font* font = node->GetStyleFont();

	node->size.y = font->glyphHeight + 4;
	node->size.x = node->size.y;

	for (OptionNode::Data* option = data->firstOption; option; option = option->next)
	{
		if (option->node->size.x > node->size.x)
		{
			node->size.x = option->node->size.x;
		}
	}

	node->size.x += 8 + Assets.downIcon->width;

	if (layout.AvailableWidth() < node->size.x)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

Node* OptionNode::Construct(Allocator& allocator)
{
	OptionNode::Data* data = allocator.Alloc<OptionNode::Data>();
	if (data)
	{
		Node* result = allocator.Alloc<Node>(Node::Option, data);
		data->node = result;
		return result;
	}
	return nullptr;
}

void OptionNode::GenerateLayout(Layout& layout, Node* node)
{
	OptionNode::Data* data = static_cast<OptionNode::Data*>(node->data);
	if (!data->addedToSelectNode)
	{
		SelectNode::Data* select = node->FindParentDataOfType<SelectNode::Data>(Node::Select);
		if (select)
		{
			if (!select->firstOption)
			{
				select->firstOption = data;
			}
			else
			{
				for (OptionNode::Data* option = select->firstOption; option; option = option->next)
				{
					if (!option->next)
					{
						option->next = data;
						break;
					}
				}
			}

			if(!select->selected)
			{
				select->selected = data;			
			}

			data->addedToSelectNode = true;
		}

		Font* font = node->GetStyleFont();

		if (data->text)
		{
			node->size.x = font->CalculateWidth(data->text, node->GetStyle().fontStyle);
		}
	}
}

bool SelectNode::HandleEvent(Node* node, const Event& event)
{
	AppInterface& ui = App::Get().ui;
	SelectNode::Data* data = static_cast<SelectNode::Data*>(node->data);

	switch (event.type)
	{
		case Event::MouseClick:
		{
			if (node == dropDownMenu.activeNode)
			{
				int clickX = event.x;
				int clickY = event.y + ui.GetScrollPositionY() - ui.windowRect.y;
				if (node->IsPointInsideNode(clickX, clickY))
				{
					ui.FocusNode(nullptr);
				}
				else
				{
					if (clickX >= dropDownMenu.rect.x && clickX < dropDownMenu.rect.x + dropDownMenu.rect.width
						&& clickY >= dropDownMenu.rect.y && clickY < dropDownMenu.rect.y + dropDownMenu.rect.height)
					{
						Font* font = node->GetStyleFont();
						int index = (clickY - dropDownMenu.rect.y + 1) / font->glyphHeight;

						for (OptionNode::Data* option = data->firstOption; option; option = option->next)
						{
							if (!index)
							{
								data->selected = option;
								break;
							}
							index--;
						}

						ui.FocusNode(nullptr);
						node->Redraw();
					}
				}
			}
			else
			{
				ShowDropDownMenu(node);
				ui.FocusNode(node);
			}
			//if (data->selected)
			//{
			//	data->selected = data->selected->next;
			//	if (!data->selected)
			//	{
			//		data->selected = data->firstOption;
			//	}
			//	node->Redraw();
			//}
		}
		return true;
		case Event::Focus:
			if (dropDownMenu.activeNode == node)
			{
				DrawDropDownMenu();
			}
			else
			{
				DrawHighlight(node, Platform::video->colourScheme.textColour);
			}
			return true;
		case Event::Unfocus:
			if (dropDownMenu.activeNode == node)
			{
				CloseDropDownMenu();
			}
			else
			{
				DrawHighlight(node, Platform::video->colourScheme.pageColour);
			}
			return true;

		case Event::KeyPress:
			switch (event.key)
			{
			case KEYCODE_ARROW_UP:
			case KEYCODE_ARROW_LEFT:
				if (data->selected)
				{
					if (data->selected == data->firstOption)
					{
						while (data->selected->next)
						{
							data->selected = data->selected->next;
						}
					}
					else
					{
						for (OptionNode::Data* option = data->firstOption; option; option = option->next)
						{
							if (option->next == data->selected)
							{
								data->selected = option;
								break;
							}
						}
					}
					node->Redraw();
				}
				return true;
			case KEYCODE_ARROW_DOWN:
			case KEYCODE_ARROW_RIGHT:
				if (data->selected)
				{
					data->selected = data->selected->next;
					if (!data->selected)
					{
						data->selected = data->firstOption;
					}
					node->Redraw();
				}
				return true;

			case KEYCODE_PAGE_UP:
			case KEYCODE_PAGE_DOWN:
			case KEYCODE_HOME:
			case KEYCODE_END:
				return node == dropDownMenu.activeNode;
			}
			break;

	}

	return false;
}

Node* SelectNode::Pick(Node* node, int x, int y)
{
	if (node && node == dropDownMenu.activeNode)
	{
		if (x >= dropDownMenu.rect.x && y >= dropDownMenu.rect.y && x < dropDownMenu.rect.x + dropDownMenu.rect.width && y < dropDownMenu.rect.y + dropDownMenu.rect.height)
		{
			return node;
		}
	}

	if (node && (!node->size.IsZero() && node->IsPointInsideNode(x, y)))
	{
		return node;
	}

	return nullptr;
}

void SelectNode::DrawDropDownMenu()
{
	Font* font = dropDownMenu.activeNode->GetStyleFont();
	SelectNode::Data* data = static_cast<SelectNode::Data*>(dropDownMenu.activeNode->data);

	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, dropDownMenu.activeNode);

	Platform::input->HideMouse();
	uint8_t clearColour = Platform::video->colourScheme.pageColour;
	uint8_t textColour = Platform::video->colourScheme.textColour;

	context.surface->FillRect(context, dropDownMenu.rect.x, dropDownMenu.rect.y, dropDownMenu.rect.width, dropDownMenu.rect.height, clearColour);
	context.surface->HLine(context, dropDownMenu.rect.x, dropDownMenu.rect.y, dropDownMenu.rect.width, textColour);
	context.surface->HLine(context, dropDownMenu.rect.x, dropDownMenu.rect.y + dropDownMenu.rect.height - 1, dropDownMenu.rect.width, textColour);
	context.surface->VLine(context, dropDownMenu.rect.x, dropDownMenu.rect.y + 1, dropDownMenu.rect.height - 2, textColour);
	context.surface->VLine(context, dropDownMenu.rect.x + dropDownMenu.rect.width - 1, dropDownMenu.rect.y + 1, dropDownMenu.rect.height - 2, textColour);

	int optionY = dropDownMenu.rect.y + 1;
	for (OptionNode::Data* option = data->firstOption; option; option = option->next)
	{
		if (option == data->selected)
		{
			context.surface->FillRect(context, dropDownMenu.rect.x + 1, optionY, dropDownMenu.rect.width - 2, font->glyphHeight, textColour);
			context.surface->DrawString(context, font, option->text, dropDownMenu.rect.x + 2, optionY, clearColour);
		}
		else
		{
			context.surface->DrawString(context, font, option->text, dropDownMenu.rect.x + 2, optionY, textColour);
		}
		optionY += font->glyphHeight;
	}

	Platform::input->ShowMouse();
}

void SelectNode::ShowDropDownMenu(Node *node)
{
	dropDownMenu.activeNode = node;
	dropDownMenu.numOptions = 0;

	SelectNode::Data* data = static_cast<SelectNode::Data*>(dropDownMenu.activeNode->data);

	for (OptionNode::Data* option = data->firstOption; option; option = option->next)
	{
		dropDownMenu.numOptions++;
	}

	AppInterface& ui = App::Get().ui;
	Rect& windowRect = ui.windowRect;
	Font* font = node->GetStyleFont();
	dropDownMenu.rect.height = font->glyphHeight * dropDownMenu.numOptions + 2;
	dropDownMenu.rect.width = dropDownMenu.activeNode->size.x;
	dropDownMenu.rect.x = dropDownMenu.activeNode->anchor.x;
	dropDownMenu.rect.y = dropDownMenu.activeNode->anchor.y + dropDownMenu.activeNode->size.y - 1;

	int offsetY = windowRect.y - ui.GetScrollPositionY();
	if (dropDownMenu.rect.y + dropDownMenu.rect.height + offsetY > windowRect.y + windowRect.height)
	{
		// Cut off the bottom of the screen

		int topPosition = dropDownMenu.activeNode->anchor.y - dropDownMenu.rect.height + 1;
		if (topPosition + offsetY < windowRect.y)
		{
			// Still cut off
			int topCutoff = windowRect.y - (topPosition + offsetY);
			int bottomCutoff = (dropDownMenu.rect.y + dropDownMenu.rect.height + offsetY) - (windowRect.y + windowRect.height);

			// TODO: add scroll bars and fit to window
			if (topCutoff < bottomCutoff)
			{
				dropDownMenu.rect.y = topPosition;
			}
			else
			{

			}
		}
		else
		{
			dropDownMenu.rect.y = topPosition;
		}
	}

	App::Get().pageRenderer.SetPaused(true);
}

void SelectNode::CloseDropDownMenu()
{
	if (dropDownMenu.activeNode)
	{
		int offsetY = App::Get().ui.windowRect.y - App::Get().ui.GetScrollPositionY();
		App::Get().pageRenderer.MarkScreenRegionDirty(dropDownMenu.rect.x, dropDownMenu.rect.y + offsetY, dropDownMenu.rect.x + dropDownMenu.rect.width, dropDownMenu.rect.y + dropDownMenu.rect.height + offsetY);
		dropDownMenu.activeNode = nullptr;
		App::Get().pageRenderer.SetPaused(false);
	}
}
