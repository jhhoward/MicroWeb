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

	class Data
	{
	public:
		Data(SectionElement::Type inType) : type(inType) { }
		SectionElement::Type type;
	};

	static Node* Construct(Allocator& allocator, SectionElement::Type sectionType);
};

