#pragma once

#include "../Node.h"

class LinkNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(char* inURL) : url(inURL) {}
		char* url;
	};

	virtual void ApplyStyle(Node* node) override;
	virtual bool CanFocus(Node* node) override { return true; }
	virtual bool HandleEvent(Node* node, const Event& event) override;

	static Node* Construct(Allocator& allocator, char* url);
};