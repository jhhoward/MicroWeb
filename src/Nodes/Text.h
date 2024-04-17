#pragma once

#include "../Node.h"
#include "../Memory/MemBlock.h"

class TextElement : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(MemBlockHandle& inText) : text(inText), lastAvailableWidth(-1) {}
		MemBlockHandle text;
		int lastAvailableWidth;
	};
	
	static Node* Construct(Allocator& allocator, const char* text);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* element) override;
};

class SubTextElement : public TextElement
{
public:
	class Data
	{
	public:
		Data(int inStartIndex, int inLength) : startIndex(inStartIndex), length(inLength) {}
		int startIndex;
		int length;
	};
	static Node* Construct(Allocator& allocator, int startIndex, int length);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* element) override;
};
