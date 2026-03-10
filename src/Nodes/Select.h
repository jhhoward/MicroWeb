#ifndef _SELET_H_
#define _SELET_H_

#include "../Node.h"

class OptionNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data() : Node(Node::Option), text(nullptr), next(nullptr), addedToSelectNode(false) {}
		const char* text;
		Data* next;
		bool addedToSelectNode;
	};

	static OptionNode::Data* Construct(Allocator& allocator);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
};

class SelectNode : public NodeHandler
{
public:
	class Data : public Node
	{
	public:
		Data(const char* inName) : Node(Node::Select), name(inName), firstOption(nullptr), selected(nullptr) {}
		const char* name;
		OptionNode::Data* firstOption;
		OptionNode::Data* selected;
	};

	virtual void ApplyStyle(Node* node) override;
	static SelectNode::Data* Construct(Allocator& allocator, const char* inName);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;

	virtual bool CanPick(Node* node) { return true; }
	virtual bool HandleEvent(Node* node, const Event& event);
	virtual Node* Pick(Node* node, int x, int y) override;

	void DrawHighlight(Node* node, uint8_t colour);

	void DrawDropDownMenu();
	void ShowDropDownMenu(Node *node);
	void CloseDropDownMenu();

	struct DropDownMenu
	{
		Node* activeNode;
		int numOptions;
		Rect rect;
	};

	DropDownMenu dropDownMenu;
};

#endif
