#pragma once

#include <stdint.h>
#include "../Node.h"
#include "../Font.h"
#include "../Style.h"

class StyleNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data() : Node(Node::Style) {}
		ElementStyleOverride styleOverride;
	};

	virtual void ApplyStyle(Node* node) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;

	static StyleNode::Data* Construct(Allocator& allocator);
	static StyleNode::Data* ConstructFontStyle(Allocator& allocator, FontStyle::Type fontStyle, int fontSize = -1);
	static StyleNode::Data* ConstructFontSize(Allocator& allocator, int fontSize);
	static StyleNode::Data* ConstructAlignment(Allocator& allocator, ElementAlignment::Type alignment);
};

