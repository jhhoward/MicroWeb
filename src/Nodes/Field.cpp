#include "Field.h"
#include "../DataPack.h"
#include "../Draw/Surface.h"
#include "../LinAlloc.h"
#include "../Layout.h"
#include "../Platform.h"

void TextFieldNode::Draw(DrawContext& context, Node* node)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);

	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	uint8_t textColour = 0;
	uint8_t buttonOutlineColour = 0;
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y, node->size.x - 2, buttonOutlineColour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 1, node->size.x - 2, buttonOutlineColour);
	context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
	context.surface->DrawString(context, font, data->buffer, node->anchor.x + 3, node->anchor.y + 2, textColour, node->style.fontStyle);
}

Node* TextFieldNode::Construct(Allocator& allocator, const char* inValue)
{
	char* buffer = (char*) allocator.Alloc(DEFAULT_TEXT_FIELD_BUFFER_SIZE);
	if (!buffer)
	{
		return nullptr;
	}

	TextFieldNode::Data* data = allocator.Alloc<TextFieldNode::Data>(buffer, DEFAULT_TEXT_FIELD_BUFFER_SIZE);
	if (data)
	{
		if (inValue)
		{
			strncpy(buffer, inValue, DEFAULT_TEXT_FIELD_BUFFER_SIZE);
		}
		else
		{
			buffer[0] = '\0';
		}
		return allocator.Alloc<Node>(Node::TextField, data);
	}

	return nullptr;
}

void TextFieldNode::GenerateLayout(Layout& layout, Node* node)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	node->size.x = Platform::video->screenWidth / 3;
	node->size.y = font->glyphHeight + 4;

	if (layout.AvailableWidth() < node->size.x)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

Node* TextFieldNode::Pick(Node* node, int x, int y)
{
	if (node->IsPointInsideNode(x, y))
	{
		return node;
	}
	return nullptr;
}
