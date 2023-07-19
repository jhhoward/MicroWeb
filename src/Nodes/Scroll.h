#ifndef _SCROLL_H_
#define _SCROLL_H_

#include "../Node.h"

class ScrollBarNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(int inScrollPosition, int inMaxScroll) : scrollPosition(inScrollPosition), maxScroll(inMaxScroll) {  }
		int scrollPosition;
		int maxScroll;
	};

	static Node* Construct(Allocator& allocator, int scrollPosition = 0, int maxScroll = 0);
	virtual void Draw(DrawContext& context, Node* node) override;
};

#endif
