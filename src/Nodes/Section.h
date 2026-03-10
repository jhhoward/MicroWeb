#pragma once

#include "../Node.h"

class SectionElement : public NodeHandler
{
public:
	enum Type
	{
		Document,
		HTML,
		Head,
		Body,
		Script,
		Style,
		Title,
		Interface
	};

	class Data : public Node
	{
	public:
		Data(SectionElement::Type inType) : Node(Node::Section), type(inType) { }
		SectionElement::Type type;
	};

	static SectionElement::Data* Construct(Allocator& allocator, SectionElement::Type sectionType);
};

