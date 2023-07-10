#include "Page.h"
#include "Node.h"
#include "Nodes/Text.h"
#include "Nodes/Section.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Break.h"
#include "Nodes/StyNode.h"
#include "Nodes/LinkNode.h"
#include "Nodes/Block.h"
#include "Nodes/Button.h"
#include "Nodes/Field.h"

NodeHandler* Node::nodeHandlers[Node::NumNodeTypes] =
{
	new SectionElement(),
	new TextElement(),
	new SubTextElement(),
	new ImageNode(),
	new BreakNode(),
	new StyleNode(),
	new LinkNode(),
	new BlockNode(),
	new ButtonNode(),
	new TextFieldNode()
};

Node::Node(Type inType, void* inData)
	: type(inType)
	, parent(nullptr)
	, next(nullptr)
	, firstChild(nullptr)
	, data(inData)
{
	anchor.Clear();
	size.Clear();
}

void Node::AddChild(Node* child)
{
	child->parent = this;
	child->style = style;

	if(!firstChild)
	{
		firstChild = child;
	}
	else
	{
		Node* lastChild;
		for(lastChild = firstChild; lastChild->next; lastChild = lastChild->next) {}
		lastChild->next = child;
	}
}

void Node::EncapsulateChildren()
{
	if (firstChild)
	{
		anchor = firstChild->anchor;
		size = firstChild->size;

		for (Node* node = firstChild->next; node; node = node->next)
		{
			if (node->anchor.x < anchor.x)
			{
				anchor.x = node->anchor.x;
			}
			if (node->anchor.y < anchor.y)
			{
				anchor.y = node->anchor.y;
			}
			if (node->anchor.x + node->size.x > anchor.x + size.x)
			{
				size.x = node->anchor.x + node->size.x - anchor.x;
			}
			if (node->anchor.y + node->size.y > anchor.y + size.y)
			{
				size.y = node->anchor.y + node->size.y - anchor.y;
			}
		}
	}
}

bool Node::IsPointInsideNode(int x, int y)
{
	return x >= anchor.x && y >= anchor.y && x < anchor.x + size.x && y < anchor.y + size.y;
}

Node* NodeHandler::Pick(Node* node, int x, int y)
{
	if (!node || !node->IsPointInsideNode(x, y))
	{
		return nullptr;
	}

	if (CanPick(node))
	{
		return node;
	}

	for (Node* it = node->firstChild; it; it = it->next)
	{
		if (it->IsPointInsideNode(x, y))
		{
			return it->Handler().Pick(it, x, y);
		}
	}

	return nullptr;
}

void NodeHandler::EndLayoutContext(Layout& layout, Node* node)
{
	if (node->size.x == 0 && node->size.y == 0)
	{
		node->EncapsulateChildren();
	}
}
