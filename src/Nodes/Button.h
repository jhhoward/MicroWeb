#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "../Node.h"

class ButtonNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(const char* inButtonText, NodeCallbackFunction inOnClick) : buttonText(inButtonText), onClick(inOnClick) {}
		const char* buttonText;
		NodeCallbackFunction onClick;
	};

	static Node* Construct(Allocator& allocator, const char* buttonText, NodeCallbackFunction onClick);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;
	virtual bool CanPick(Node* node) { return true; }
	virtual bool HandleEvent(Node* node, const Event& event);

	static Coord CalculateSize(Node* node);
};

#endif
