#include "Page.h"
#include "Node.h"
#include "Nodes/Text.h"
#include "Nodes/Section.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Break.h"
#include "Nodes/StyNode.h"

NodeHandler* Node::nodeHandlers[Node::NumNodeTypes] =
{
	new SectionElement(),
	new TextElement(),
	new SubTextElement(),
	new ImageNode(),
	new BreakNode(),
	new StyleNode()
};

Node::Node(Type inType, void* inData)
	: type(inType)
	, style(0)
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


