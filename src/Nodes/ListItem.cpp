#include <stdio.h>
#include "../Layout.h"
#include "../Memory/LinAlloc.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"
#include "ListItem.h"

Node* ListNode::Construct(Allocator& allocator)
{
	ListNode::Data* data = allocator.Alloc<ListNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::List, data);
	}
	return nullptr;
}

void ListNode::BeginLayoutContext(Layout& layout, Node* node)
{
	ListNode::Data* data = static_cast<ListNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	layout.BreakNewLine();
	layout.PadVertical(font->glyphHeight);
	layout.PushLayout();
	layout.PadHorizontal(16, 0);
}

void ListNode::EndLayoutContext(Layout& layout, Node* node)
{
	ListNode::Data* data = static_cast<ListNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	layout.PopLayout();
	layout.BreakNewLine();
	layout.PadVertical(font->glyphHeight);
}

Node* ListItemNode::Construct(Allocator& allocator)
{
	ListItemNode::Data* data = allocator.Alloc<ListItemNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::ListItem, data);
	}
	return nullptr;
}

void ListItemNode::Draw(DrawContext& context, Node* node)
{
	ListItemNode::Data* data = static_cast<ListItemNode::Data*>(node->data);

	if (data)
	{
		context.surface->BlitImage(context, Assets.bulletIcon, node->anchor.x, node->anchor.y);
	}
}


void ListItemNode::BeginLayoutContext(Layout& layout, Node* node)
{
	ListItemNode::Data* data = static_cast<ListItemNode::Data*>(node->data);

	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	layout.BreakNewLine();
	node->anchor = layout.GetCursor();
	node->anchor.y += (font->glyphHeight - Assets.bulletIcon->height) / 2;
	node->size.x = layout.AvailableWidth();
	layout.PushLayout();
	layout.PadHorizontal(Assets.bulletIcon->width * 2, 0);
}

void ListItemNode::EndLayoutContext(Layout& layout, Node* node)
{
	ListItemNode::Data* data = static_cast<ListItemNode::Data*>(node->data);

	layout.PopLayout();
	layout.BreakNewLine();

	node->size.y = layout.Cursor().y - node->anchor.y;
}
