#include "StyNode.h"
#include "../Memory/Memory.h"
#include "../Layout.h"

void StyleNode::ApplyStyle(Node* node)
{
	StyleNode::Data* data = static_cast<StyleNode::Data*>(node);
	ElementStyle style = node->GetStyle();
	data->styleOverride.Apply(style);
	node->SetStyle(style);
}

void StyleNode::GenerateLayout(Layout& layout, Node* node)
{
	StyleNode::Data* data = static_cast<StyleNode::Data*>(node);
	if (data->styleOverride.overrideMask.alignment)
	{
		layout.BreakNewLine();
	}
}

StyleNode::Data* StyleNode::Construct(Allocator& allocator)
{
	return allocator.Alloc<StyleNode::Data>();
}

StyleNode::Data* StyleNode::ConstructFontStyle(Allocator& allocator, FontStyle::Type fontStyle, int fontSize)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		data->styleOverride.SetFontStyle(fontStyle);
		if (fontSize >= 0)
		{
			data->styleOverride.SetFontSize(fontSize);
		}
		return data;
	}
	return nullptr;
}

StyleNode::Data* StyleNode::ConstructFontSize(Allocator& allocator, int fontSize)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		data->styleOverride.SetFontSize(fontSize);
		return data;
	}
	return nullptr;
}

StyleNode::Data* StyleNode::ConstructAlignment(Allocator& allocator, ElementAlignment::Type alignment)
{
	StyleNode::Data* data = allocator.Alloc<StyleNode::Data>();
	if (data)
	{
		data->styleOverride.SetAlignment(alignment);
		return data;
	}
	return nullptr;
}
