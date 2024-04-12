#pragma once
#ifndef _NODE_H_
#define _NODE_H_

#include <stdint.h>
#include "Style.h"
#include "Event.h"
#include "Defines.h"
#include "Memory/Alloc.h"

class App;
class Page;
class Node;
class Layout;
struct DrawContext;

class NodeHandler
{
public:
	virtual void Draw(DrawContext& context, Node* element) {}
	virtual void GenerateLayout(Layout& layout, Node* node) {}
	virtual void BeginLayoutContext(Layout& layout, Node* node) {}
	virtual void EndLayoutContext(Layout& layout, Node* node);
	virtual void ApplyStyle(Node* node) {}
	virtual Node* Pick(Node* node, int x, int y);
	virtual bool CanPick(Node* node) { return false; }
	virtual bool HandleEvent(Node* node, const Event& event) { return false; }

	virtual void LoadContent(Node* node, struct LoadTask& loadTask) {}
	virtual bool ParseContent(Node* node, char* buffer, size_t count) { return false; }
	virtual void FinishContent(Node* node, struct LoadTask& loadTask) {}

};

struct Coord
{
	int16_t x, y;

	void Clear() { x = y = 0; }
	bool IsZero() const { return x == 0 && y == 0; }
};

struct Rect
{
	int16_t x, y, width, height;

	void Clear() { x = y = width = height = 0; }
};

// For when width / height are set on an element
// percentage is stored internally as a negative number, px as positive, zero is no value set
struct ExplicitDimension
{
	static ExplicitDimension Parse(const char* str);

	ExplicitDimension() : value(0) {}
	bool IsSet() { return value != 0; }
	bool IsPercentage() { return value < 0; }
	int16_t Value() { return value < 0 ? -value : value; }

private:
	int16_t value;
};

#pragma pack(push, 1)
class Node
{
public:
	enum Type
	{
		Section,
		Text,
		SubText,
		Image,
		Break,
		Style,
		Link,
		Block,
		Button,
		TextField,
		Form,
		StatusBar,
		ScrollBar,
		Table,
		TableRow,
		TableCell,
		Select,
		Option,
		List,
		ListItem,
		CheckBox,
		NumNodeTypes
	};

	NodeHandler& Handler() const
	{
		return *nodeHandlers[type];
	}

	Node(Type inType, void* inData);
	void AddChild(Node* child);
	void InsertSibling(Node* sibling);
	void CalculateEncapsulatingRect(Rect& rect);
	bool IsPointInsideNode(int x, int y);
	bool IsPointInsideChildren(int x, int y);
	Node* FindParentOfType(Node::Type searchType);
	template <typename T>
	T* FindParentDataOfType(Node::Type searchType)
	{
		Node* parent = FindParentOfType(searchType);
		if (parent)
		{
			return static_cast<T*>(parent->data);
		}
		return nullptr;
	}
	bool IsChildOf(Node* node);

	const ElementStyle& GetStyle();
	void SetStyle(const ElementStyle& style);
	Font* GetStyleFont();

	void Redraw();

	Node* GetPreviousInTree();
	Node* GetNextInTree();

	ElementStyleHandle styleHandle;
	Type type : 7;
	bool isLayoutComplete : 1;

	Coord anchor;				// Top left page position
	Coord size;					// Rectangle size that encapsulates node and its children

	Node* parent;
	Node* next;
	Node* firstChild;

	void* data;

protected:
	static NodeHandler* nodeHandlers[Node::NumNodeTypes];
};
#pragma pack(pop)

typedef void(*NodeCallbackFunction)(Node* node);


#endif
