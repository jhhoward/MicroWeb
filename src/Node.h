#pragma once
#ifndef _NODE_H_
#define _NODE_H_

#include <stdint.h>
#include "Style.h"

class Page;
class Node;
class Layout;
typedef class LinearAllocator Allocator;

class NodeHandler
{
public:
	virtual void Draw(Page& page, Node* element) {}
	virtual void GenerateLayout(Layout& layout, Node* node) {}
	virtual void ApplyStyle(Node* node) {}
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
		NumNodeTypes
	};

	NodeHandler& Handler() const
	{
		return *nodeHandlers[type];
	}

	Node(Type inType, void* inData);
	void AddChild(Node* child);

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

#endif
