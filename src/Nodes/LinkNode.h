#pragma once

#include "../Node.h"
#include "../Memory/MemBlock.h"

class LinkNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data(MemBlockHandle& inURL) : Node(Node::Link), url(inURL) {}

		const char* GetURL() { return url.Get<char*>(); }

	private:
		MemBlockHandle url;
	};

	virtual void ApplyStyle(Node* node) override;
	virtual bool CanPick(Node* node) override { return true; }
	virtual bool HandleEvent(Node* node, const Event& event) override;

	static LinkNode::Data* Construct(Allocator& allocator, const char* url);

	void HighlightChildren(Node* node);

};