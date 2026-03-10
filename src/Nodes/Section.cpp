
#include "Section.h"
#include "../Memory/Memory.h"
#include "../Layout.h"

SectionElement::Data* SectionElement::Construct(Allocator& allocator, SectionElement::Type sectionType)
{
	return allocator.Alloc<SectionElement::Data>(sectionType);
}

