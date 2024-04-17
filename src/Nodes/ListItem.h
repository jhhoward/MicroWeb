#ifndef _LISTITEM_H_
#define _LISTITEM_H_

#include "../Node.h"

class ListItemNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data()  {}
		int index;
	};

	static Node* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

class ListNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() {}
		char type;
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

#endif
