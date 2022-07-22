#include "../Page.h"
#include "Text.h"
#include "../LinAlloc.h"

void TextElement::Draw(Page& page, Node* element)
{
	TextElement::Data* data = static_cast<TextElement::Data*>(element->data);
}


Node* TextElement::Construct(Allocator& allocator, const char* text)
{
	const char* textString = allocator.AllocString(text);
	if (textString)
	{
		TextElement::Data* data = allocator.Alloc<TextElement::Data>(textString);
		if (data)
		{
			return allocator.Alloc<Node>(Node::Type::Text, data);
		}
	}

	return nullptr;
}
