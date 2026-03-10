#pragma once

#include "../Node.h"

class LinkNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data(char* inURL) : Node(Node::Link), url(inURL) {}
		char* url;
	};

	virtual void ApplyStyle(Node* node) override;
	virtual bool CanPick(Node* node) override { return true; }
	virtual bool HandleEvent(Node* node, const Event& event) override;

	static LinkNode::Data* Construct(Allocator& allocator, char* url);

	void HighlightChildren(Node* node);

};