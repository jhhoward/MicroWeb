#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "../Node.h"

typedef void(*OnButtonNodeClickedCallback)(App& app, Node* node);

class ButtonNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(const char* inButtonText, OnButtonNodeClickedCallback inCallback) : buttonText(inButtonText), callback(inCallback) {}
		const char* buttonText;
		OnButtonNodeClickedCallback callback;
	};

	static Node* Construct(Allocator& allocator, const char* buttonText, OnButtonNodeClickedCallback callback);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;
	virtual bool CanPick(Node* node) { return true; }
	virtual bool HandleEvent(Node* node, const Event& event);

	static Coord CalculateSize(Node* node);
};

#endif
