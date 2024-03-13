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
};

struct Coord
{
	int16_t x, y;

	void Clear() { x = y = 0; }
};

struct Rect
{
	int16_t x, y, width, height;

	void Clear() { x = y = width = height = 0; }
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
		NumNodeTypes
	};

	NodeHandler& Handler() const
	{
		return *nodeHandlers[type];
	}

	Node(Type inType, void* inData);
	void AddChild(Node* child);
	void EncapsulateChildren();		// Sets anchor and size based on children
	bool IsPointInsideNode(int x, int y);
	bool IsPointInsideChildren(int x, int y);
	void OnChildLayoutChanged();
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

	void Redraw();

	Node* GetPreviousInTree();
	Node* GetNextInTree();

	ElementStyle style;
	Type type : 8;

	Coord anchor;				// Top left page position
	Coord size;					// Rectangle size that encapsulates node and its children

	Node* parent;
	Node* next;
	Node* firstChild;

	Node* nextNodeToRender;

	void* data;

protected:
	static NodeHandler* nodeHandlers[Node::NumNodeTypes];
};
#pragma pack(pop)

typedef void(*NodeCallbackFunction)(Node* node);


#endif
