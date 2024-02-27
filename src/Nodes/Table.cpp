#include <stdio.h>
#include "../Layout.h"
#include "../Memory/Memory.h"
#include "Table.h"

Node* TableNode::Construct(Allocator& allocator)
{
	TableNode::Data* data = allocator.Alloc<TableNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Table, data);
	}
	return nullptr;
}

void TableNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableNode::Data* data = static_cast<TableNode::Data*>(node->data);

	layout.BreakNewLine();
//	layout.PadVertical(data->verticalPadding);
	layout.PushLayout();
	//layout.PadHorizontal(data->horizontalPadding, data->horizontalPadding);
}

void TableNode::EndLayoutContext(Layout& layout, Node* node)
{
	NodeHandler::EndLayoutContext(layout, node);

	TableNode::Data* data = static_cast<TableNode::Data*>(node->data);

	layout.PopLayout();
	layout.BreakNewLine();
	//layout.PadVertical(data->verticalPadding);
}

// Table row node

Node* TableRowNode::Construct(Allocator& allocator)
{
	TableRowNode::Data* data = allocator.Alloc<TableRowNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::TableRow, data);
	}
	return nullptr;
}

void TableRowNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableRowNode::Data* data = static_cast<TableRowNode::Data*>(node->data);

	layout.BreakNewLine();
	//	layout.PadVertical(data->verticalPadding);
	layout.PushLayout();
	//layout.PadHorizontal(data->horizontalPadding, data->horizontalPadding);
}

void TableRowNode::EndLayoutContext(Layout& layout, Node* node)
{
	NodeHandler::EndLayoutContext(layout, node);

	TableRowNode::Data* data = static_cast<TableRowNode::Data*>(node->data);

	layout.PopLayout();
	layout.BreakNewLine();
	layout.PadVertical(node->size.y);
	//layout.PadVertical(data->verticalPadding);
}

// Table cell node

Node* TableCellNode::Construct(Allocator& allocator)
{
	TableCellNode::Data* data = allocator.Alloc<TableCellNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::TableCell, data);
	}
	return nullptr;
}

void TableCellNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	//layout.BreakNewLine();
	//	layout.PadVertical(data->verticalPadding);
	layout.PushLayout();
	layout.RestrictHorizontal(200);
	layout.PushCursor();
	//layout.PadHorizontal(data->horizontalPadding, data->horizontalPadding);
}

void TableCellNode::EndLayoutContext(Layout& layout, Node* node)
{
	NodeHandler::EndLayoutContext(layout, node);

	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	layout.PopCursor(); 
	layout.PopLayout();
	//layout.ProgressCursor(node, 100, 0);
	layout.PadHorizontal(200, 0);
	//layout.BreakNewLine();
	//layout.PadVertical(data->verticalPadding);
}
