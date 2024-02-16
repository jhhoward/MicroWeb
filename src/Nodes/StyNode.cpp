#include "StyNode.h"
#include "../Memory/Memory.h"
#include "../Layout.h"

void StyleNode::ApplyStyle(Node* node)
{
	StyleNode::Data* data = static_cast<StyleNode::Data*>(node->data);
	data->styleOverride.Apply(node->style);
}

void StyleNode::GenerateLayout(Layout& layout, Node* node)
{
	StyleNode::Data* data = static_cast<StyleNode::Data*>(node->data);
	if (data->styleOverride.overrideMask.alignment)
	{
		layout.BreakNewLine();
	}
}

Node* StyleNode::Construct(Allocator& allocator)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Style, data);
	}
	return nullptr;
}

Node* StyleNode::ConstructFontStyle(Allocator& allocator, FontStyle::Type fontStyle, int fontSize)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		data->styleOverride.SetFontStyle(fontStyle);
		if (fontSize >= 0)
		{
			data->styleOverride.SetFontSize(fontSize);
		}
		return allocator.Alloc<Node>(Node::Style, data);
	}
	return nullptr;
}

Node* StyleNode::ConstructFontSize(Allocator& allocator, int fontSize)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		data->styleOverride.SetFontSize(fontSize);
		return allocator.Alloc<Node>(Node::Style, data);
	}
	return nullptr;
}

Node* StyleNode::ConstructAlignment(Allocator& allocator, ElementAlignment::Type alignment)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		data->styleOverride.SetAlignment(alignment);
		return allocator.Alloc<Node>(Node::Style, data);
	}
	return nullptr;
}
