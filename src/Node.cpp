#include "Page.h"
#include "App.h"
#include "Draw/Surface.h"
#include "DataPack.h"
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
#include "Nodes/Select.h"
#include "Nodes/ListItem.h"

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
	new TableCellNode(),
	new SelectNode(),
	new OptionNode(),
	new ListNode(),
	new ListItemNode()
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
	child->styleHandle = styleHandle;

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

void Node::InsertSibling(Node* sibling)
{
	sibling->styleHandle = styleHandle;
	sibling->parent = parent;
	sibling->next = next;
	next = sibling;
}

void Node::CalculateEncapsulatingRect(Rect& rect)
{
	rect.Clear();
	Node* node = this;
	bool checkChildren = true;

	if (!firstChild)
	{
		rect.x = anchor.x;
		rect.y = anchor.y;
		rect.width = size.x;
		rect.height = size.y;
		return;
	}

	while (node)
	{
		if (checkChildren && node->firstChild)
		{
			node = node->firstChild;
		}
		else if (node->next)
		{
			node = node->next;
			checkChildren = true;
		}
		else
		{
			node = node->parent;
			if (node == this)
			{
				break;
			}
			checkChildren = false;
		}

		if (node->size.x == 0 || node->size.y == 0)
			continue;

		if (rect.width == 0 && rect.height == 0)
		{
			rect.x = node->anchor.x;
			rect.y = node->anchor.y;
			rect.width = node->size.x;
			rect.height = node->size.y;
			continue;
		}

		if (node->anchor.x < rect.x)
		{
			rect.x = node->anchor.x;
		}
		if (node->anchor.y < rect.y)
		{
			rect.y = node->anchor.y;
		}
		if (node->anchor.x + node->size.x > rect.x + rect.width)
		{
			rect.width = node->anchor.x + node->size.x - rect.x;
		}
		if (node->anchor.y + node->size.y > rect.y + rect.height)
		{
			rect.height = node->anchor.y + node->size.y - rect.y;
		}
	}
}


bool Node::IsPointInsideNode(int x, int y)
{
	return x >= anchor.x && y >= anchor.y && x < anchor.x + size.x && y < anchor.y + size.y;
}

bool Node::IsPointInsideChildren(int x, int y)
{
	if (!size.IsZero() && !IsPointInsideNode(x, y))
	{
		return false;
	}

	if (firstChild == nullptr)
	{
		return IsPointInsideNode(x, y);
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
	if (!node || (!node->size.IsZero() && !node->IsPointInsideNode(x, y)))
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

		return nullptr;
	}

	for (Node* it = node->firstChild; it; it = it->next)
	{
		Node* result = it->Handler().Pick(it, x, y);
		if (result)
		{
			return result;
		}
	}

	return nullptr;
}

void NodeHandler::EndLayoutContext(Layout& layout, Node* node)
{
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

Node* Node::GetPreviousInTree()
{
	if (parent)
	{
		if (parent->firstChild == this)
		{
			return parent;
		}

		for (Node* child = parent->firstChild; child; child = child->next)
		{
			if (child->next == this)
			{
				Node* node = child;

				while (node->firstChild)
				{
					node = node->firstChild;

					while (node->next)
					{
						node = node->next;
					}
				}

				return node;
			}
		}

		// Shouldn't ever get here
		return nullptr;
	}
	else
	{
		// Top of the tree
		return nullptr;
	}
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

bool Node::IsChildOf(Node* potentialParent)
{
	for(Node* node = parent; node; node = node->parent)
	{
		if (node == potentialParent)
		{
			return true;
		}
	}

	return false;
}

ExplicitDimension ExplicitDimension::Parse(const char* str)
{
	ExplicitDimension result;

	char* unit;
	long value = strtol(str, &unit, 10);
	if (value)
	{
		if (unit && *unit == '%')
		{
			result.value = (int16_t)(-value);
		}
		else
		{
			result.value = (int16_t) value;
		}
	}

	return result;
}

const ElementStyle& Node::GetStyle()
{
	return StylePool::Get().GetStyle(styleHandle);
}

void Node::SetStyle(const ElementStyle& style)
{
	//if(memcmp(&style, &StylePool::Get().GetStyle(styleHandle), sizeof(ElementStyle)))
	{
		styleHandle = StylePool::Get().AddStyle(style);
	}
}

Font* Node::GetStyleFont()
{
	const ElementStyle& style = GetStyle();
	return Assets.GetFont(style.fontSize, style.fontStyle);
}
