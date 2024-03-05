#pragma once

#include "../Node.h"

class TextElement : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(const char* inText) : text(const_cast<char*>(inText)), lastAvailableWidth(-1) {}
		char* text;
		int lastAvailableWidth;
	};
	
	static Node* Construct(Allocator& allocator, const char* text);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;
};

class SubTextElement : public TextElement
{
public:
	static Node* Construct(Allocator& allocator, const char* text);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
};
