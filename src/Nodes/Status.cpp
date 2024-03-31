#include "Status.h"
#include "../Memory/Memory.h"
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

	Font* font = node->GetStyleFont();
	uint8_t textColour = Platform::video->colourScheme.textColour;
	uint8_t clearColour = Platform::video->colourScheme.pageColour;
	context.surface->HLine(context, node->anchor.x, node->anchor.y, node->size.x, textColour);
	context.surface->FillRect(context, node->anchor.x, node->anchor.y + 1, node->size.x, node->size.y - 1, clearColour);

	const char* message = data->messages[1].HasMessage() ? data->messages[1].message : data->messages[0].message;
	context.surface->DrawString(context, font, message, node->anchor.x + 1, node->anchor.y + 1, textColour, node->GetStyle().fontStyle);
}

void StatusBarNode::SetStatus(Node* node, const char* message, StatusType type)
{
	StatusBarNode::Data* data = static_cast<StatusBarNode::Data*>(node->data);

	if (message)
	{
		strncpy(data->messages[type].message, message, MAX_STATUS_BAR_MESSAGE_LENGTH);
	}
	else
	{
		data->messages[type].Clear();
	}
	node->Redraw();
}
