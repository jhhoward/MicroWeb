#pragma once
#ifndef _NODE_H_
#define _NODE_H_

#include <stdint.h>
#include "Style.h"
#include "Event.h"

class App;
class Page;
class Node;
class Layout;
struct DrawContext;
typedef class LinearAllocator Allocator;

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
	Node* FindParentOfType(Node::Type searchType);
	void Redraw();

	Type type;

	ElementStyle style;
	
	Coord anchor;				// Top left page position
	Coord size;					// Rectangle size that encapsulates node and its children

	Node* parent;
	Node* next;
	Node* firstChild;

	void* data;

protected:
	static NodeHandler* nodeHandlers[Node::NumNodeTypes];
};

typedef void(*NodeCallbackFunction)(Node* node);


#endif
