#ifndef _SCROLL_H_
#define _SCROLL_H_

#include "../Node.h"

class ScrollBarNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(int inScrollPosition, int inMaxScroll, NodeCallbackFunction inOnScroll) : scrollPosition(inScrollPosition), maxScroll(inMaxScroll), onScroll(inOnScroll) {  }
		int scrollPosition;
		int maxScroll;
		NodeCallbackFunction onScroll;
	};

	static Node* Construct(Allocator& allocator, int scrollPosition = 0, int maxScroll = 0, NodeCallbackFunction onScroll = nullptr);
	virtual void Draw(DrawContext& context, Node* node) override;
	virtual bool HandleEvent(Node* node, const Event& event) override;
	virtual bool CanPick(Node* node) { return true; }

	void CalculateWidgetParams(Node* node, int& outPosition, int& outSize);

	int startDragOffset;
	int draggingScrollPosition;
};

#endif
