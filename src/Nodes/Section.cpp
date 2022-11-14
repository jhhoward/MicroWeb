
#include "Section.h"
#include "../LinAlloc.h"
#include "../Layout.h"

Node* SectionElement::Construct(Allocator& allocator, SectionElement::Type sectionType)
{
	SectionElement::Data* data = allocator.Alloc<SectionElement::Data>(sectionType);

	if (data)
	{
		return allocator.Alloc<Node>(Node::Section, data);
	}
	return nullptr;
}

