#ifndef _BREAK_H_
#define _BREAK_H_

#include "../Node.h"

class BreakNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(int inBreakPadding, bool inDisplayBreakLine, bool inOnlyPadEmptyLines) : breakPadding(inBreakPadding), displayBreakLine(inDisplayBreakLine), onlyPadEmptyLines(inOnlyPadEmptyLines) {}
		int breakPadding;
		bool displayBreakLine;
		bool onlyPadEmptyLines;
	};

	static Node* Construct(Allocator& allocator, int breakPadding = 0, bool displayBreakLine = false, bool onlyPadEmptyLines = false);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;
};

#endif
