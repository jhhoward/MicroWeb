#include "Section.h"
#include "../LinAlloc.h"

Node* SectionElement::Construct(Allocator& allocator, SectionElement::Type sectionType)
{
	SectionElement::Data* data = allocator.Alloc<SectionElement::Data>(sectionType);

	if (data)
	{
		return allocator.Alloc<Node>(Node::Type::Section, data);
	}
	return nullptr;
}
