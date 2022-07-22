#pragma once
#include <stdint.h>

class Page;
class Node;
typedef class LinearAllocator Allocator;

typedef uint16_t ElementStyle;

class NodeHandler
{
public:
	virtual void Draw(Page& page, Node* element) = 0;
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
	Coord anchor;
	Rect rect;

	Node* parent;
	Node* next;
	Node* firstChild;

	void* data;

protected:
	static NodeHandler* nodeHandlers[Node::NumNodeTypes];
};
