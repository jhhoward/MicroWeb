#ifndef _SELET_H_
#define _SELET_H_

#include "../Node.h"

class OptionNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() : node(nullptr), text(nullptr), next(nullptr), addedToSelectNode(false) {}
		Node* node;
		const char* text;
		Data* next;
		bool addedToSelectNode;
	};

	static Node* Construct(Allocator& allocator);
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

class SelectNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() : firstOption(nullptr), selected(nullptr) {}
		OptionNode::Data* firstOption;
		OptionNode::Data* selected;
	};

	static Node* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

#endif
