#ifndef _BREAK_H_
#define _BREAK_H_

#include "../Node.h"

class BreakNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(int inBreakPadding, bool inDisplayBreakLine) : breakPadding(inBreakPadding), displayBreakLine(inDisplayBreakLine) {}
		int breakPadding;
		bool displayBreakLine;
	};

	static Node* Construct(Allocator& allocator, int breakPadding = 0, bool displayBreakLine = false);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
};

#endif
