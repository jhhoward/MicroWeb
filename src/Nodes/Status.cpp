#include "Status.h"
#include "../LinAlloc.h"
#include "../DataPack.h"
#include "../App.h"
#include "../Interface.h"
#include "../Draw/Surface.h"

Node* StatusBarNode::Construct(Allocator& allocator)
{
	StatusBarNode::Data* data = allocator.Alloc<StatusBarNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::StatusBar, data);
	}

	return nullptr;
}

void StatusBarNode::Draw(DrawContext& context, Node* node)
{
	StatusBarNode::Data* data = static_cast<StatusBarNode::Data*>(node->data);

	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	uint8_t textColour = 1;
	uint8_t clearColour = 0;
	context.surface->FillRect(context, node->anchor.x, node->anchor.y, node->size.x, node->size.y, clearColour);
	context.surface->DrawString(context, font, data->message, node->anchor.x + 1, node->anchor.y + 1, textColour, node->style.fontStyle);
}

