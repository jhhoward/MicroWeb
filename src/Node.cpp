#include "Page.h"
#include "Node.h"
#include "Nodes/Text.h"
#include "Nodes/Section.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Break.h"
#include "Nodes/StyNode.h"
#include "Nodes/LinkNode.h"

NodeHandler* Node::nodeHandlers[Node::NumNodeTypes] =
{
	new SectionElement(),
	new TextElement(),
	new SubTextElement(),
	new ImageNode(),
	new BreakNode(),
	new StyleNode(),
	new LinkNode()
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


