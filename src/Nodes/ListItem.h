#ifndef _LISTITEM_H_
#define _LISTITEM_H_

#include "../Node.h"

class ListItemNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data() : Node(Node::ListItem) {}
		int index;
	};

	static ListItemNode::Data* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

class ListNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data() : Node(Node::List) {}
		char type;
	};

	static ListNode::Data* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

#endif
