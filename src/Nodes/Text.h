#pragma once

#include "../Node.h"

class TextElement : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(const char* inText) : text(inText) {}
		const char* text;
	};
	
	static Node* Construct(Allocator& allocator, const char* text);
	virtual void Draw(Page& page, Node* element) override;
};
