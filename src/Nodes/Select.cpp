#include <stdio.h>
#include "../Layout.h"
#include "../Memory/LinAlloc.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"

#include "Select.h"

Node* SelectNode::Construct(Allocator& allocator)
{
	SelectNode::Data* data = allocator.Alloc<SelectNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Select, data);
	}
	return nullptr;
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

	node->size.x += 6;

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

void OptionNode::EndLayoutContext(Layout& layout, Node* node)
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
				select->selected = data;
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

			data->addedToSelectNode = true;
		}

		Font* font = node->GetStyleFont();

		if (data->text)
		{
			node->size.x = font->CalculateWidth(data->text, node->GetStyle().fontStyle);
		}
	}
}
