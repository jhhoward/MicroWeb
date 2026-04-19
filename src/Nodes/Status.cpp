#include "Status.h"
#include "../Memory/Memory.h"
#include "../DataPack.h"
#include "../App.h"
#include "../Interface.h"
#include "../Draw/Surface.h"

StatusBarNode::Data* StatusBarNode::Construct(Allocator& allocator)
{
	return allocator.Alloc<StatusBarNode::Data>();
}

void StatusBarNode::Draw(DrawContext& context, Node* node)
{
	StatusBarNode::Data* data = static_cast<StatusBarNode::Data*>(node);

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
	StatusBarNode::Data* data = static_cast<StatusBarNode::Data*>(node);

	if (message)
	{
		if (strncmp(message, data->messages[type].message, MAX_STATUS_BAR_MESSAGE_LENGTH))
		{
			strncpy(data->messages[type].message, message, MAX_STATUS_BAR_MESSAGE_LENGTH);
			node->Redraw();
		}
	}
	else
	{
		if (data->messages[type].HasMessage())
		{
			data->messages[type].Clear();
			node->Redraw();
		}
	}
}
