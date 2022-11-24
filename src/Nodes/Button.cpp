#include "Button.h"
#include "../DataPack.h"
#include "../Draw/Surface.h"
#include "../LinAlloc.h"
#include "../Layout.h"

void ButtonNode::Draw(DrawContext& context, Node* node)
{
	ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);

	if (data->buttonText)
	{
		Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
		uint8_t textColour = 0;
		uint8_t buttonOutlineColour = 0;
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y, node->size.x - 2, buttonOutlineColour);
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 1, node->size.x - 2, buttonOutlineColour);
		context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
		context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
		context.surface->DrawString(context, font, data->buttonText, node->anchor.x + 8, node->anchor.y + 2, textColour, node->style.fontStyle);
	}
}

Node* ButtonNode::Construct(Allocator& allocator, const char* inButtonText)
{
	const char* buttonText = NULL;
	if (inButtonText)
	{
		buttonText = allocator.AllocString(inButtonText);
		if (!buttonText)
		{
			return nullptr;
		}
	}

	ButtonNode::Data* data = allocator.Alloc<ButtonNode::Data>(buttonText);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Button, data);
	}

	return nullptr;
}

Coord ButtonNode::CalculateSize(Node* node)
{
	ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	int labelHeight = font->glyphHeight;
	int labelWidth = 0;

	if (data->buttonText)
	{
		labelWidth = font->CalculateWidth(data->buttonText);
	}

	Coord result;
	result.x = labelWidth + 16;
	result.y = labelHeight + 4;
	return result;
}

void ButtonNode::GenerateLayout(Layout& layout, Node* node)
{
	//ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);

	node->size = CalculateSize(node);

	if (layout.AvailableWidth() < node->size.x)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

Node* ButtonNode::Pick(Node* node, int x, int y)
{
	if (node->IsPointInsideNode(x, y))
	{
		return node;
	}
	return nullptr;
}
