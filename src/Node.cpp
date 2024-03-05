#include "Page.h"
#include "App.h"
#include "Draw/Surface.h"
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
#include "Nodes/Form.h"
#include "Nodes/Status.h"
#include "Nodes/Scroll.h"
#include "Nodes/Table.h"

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
	new TextFieldNode(),
	new FormNode(),
	new StatusBarNode(),
	new ScrollBarNode(),
	new TableNode(),
	new TableRowNode(),
	new TableCellNode()
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

void Node::OnChildLayoutChanged()
{
	EncapsulateChildren();
	if (parent)
	{
		parent->OnChildLayoutChanged();
	}
}

void Node::EncapsulateChildren()
{
	Coord newAnchor, newSize;
	newAnchor.Clear();
	newSize.Clear();

	for (Node* node = firstChild; node; node = node->next)
	{
		if (node->size.x == 0 || node->size.y == 0)
			continue;

		if (newSize.x == 0 || newSize.y == 0)
		{
			newAnchor = node->anchor;
			newSize = node->size;
			continue;
		}

		if (node->anchor.x < newAnchor.x)
		{
			newAnchor.x = node->anchor.x;
		}
		if (node->anchor.y < newAnchor.y)
		{
			newAnchor.y = node->anchor.y;
		}
		if (node->anchor.x + node->size.x > newAnchor.x + newSize.x)
		{
			newSize.x = node->anchor.x + node->size.x - newAnchor.x;
		}
		if (node->anchor.y + node->size.y > newAnchor.y + newSize.y)
		{
			newSize.y = node->anchor.y + node->size.y - newAnchor.y;
		}
	}

	if (newSize.x != 0 && newSize.y != 0)
	{
		anchor = newAnchor;
		size = newSize;
	}

	/*
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
	*/
}

bool Node::IsPointInsideNode(int x, int y)
{
	return x >= anchor.x && y >= anchor.y && x < anchor.x + size.x && y < anchor.y + size.y;
}

bool Node::IsPointInsideChildren(int x, int y)
{
	if (!IsPointInsideNode(x, y))
	{
		return false;
	}

	if (firstChild == nullptr)
	{
		return true;
	}

	for (Node* it = firstChild; it; it = it->next)
	{
		if (it->IsPointInsideChildren(x, y))
		{
			return true;
		}
	}

	return false;
}

Node* NodeHandler::Pick(Node* node, int x, int y)
{
	if (!node || !node->IsPointInsideNode(x, y))
	{
		return nullptr;
	}

	if (CanPick(node))
	{
		// Node is pickable, but it's dimensions could be based on encapsulated children so check 
		// it is over the child nodes and not just in the bounding box
		if (node->IsPointInsideChildren(x, y))
		{
			return node;
		}

		return nullptr;;
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
	//if (node->size.x == 0 && node->size.y == 0)
	{
		node->EncapsulateChildren();
	}
}

Node* Node::FindParentOfType(Node::Type searchType)
{
	for(Node* node = parent; node; node = node->parent)
	{
		if (node->type == searchType)
		{
			return node;
		}
	}
	return NULL;
}

void Node::Redraw()
{
	DrawContext context;

	Platform::input->HideMouse();
	App::Get().pageRenderer.GenerateDrawContext(context, this);
	Handler().Draw(context, this);

	Platform::input->ShowMouse();
}

Node* Node::GetNextInTree()
{
	Node* node = this;
	bool checkChildren = true;

	while (node)
	{
		if (checkChildren && node->firstChild)
		{
			return node->firstChild;
		}
		else if (node->next)
		{
			return node->next;
		}
		else
		{
			node = node->parent;
			checkChildren = false;
		}
	}

	return nullptr;
}
