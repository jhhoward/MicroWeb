#pragma once

#include "../Node.h"

class LinkNode : public NodeHandler
{
public:
	class Data
	{
	public:
		char* url;
	};

	virtual void ApplyStyle(Node* node) override;
	virtual bool CanPick(Node* node) override { return true; }

	static Node* Construct(Allocator& allocator);
};