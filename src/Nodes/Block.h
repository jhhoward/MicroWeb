#pragma once

#include "../Node.h"

class BlockNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data(int inHorizontalPadding, int inVerticalPadding) : Node(Node::Block), horizontalPadding(inHorizontalPadding), verticalPadding(inVerticalPadding) {}
		int horizontalPadding;
		int verticalPadding;
	};

	static BlockNode::Data* Construct(Allocator& allocator, int horizontalPadding = 0, int verticalPadding = 0);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};
