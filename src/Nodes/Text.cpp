#include "../Platform.h"
#include "../Page.h"
#include "Text.h"
#include "../LinAlloc.h"
#include "../Layout.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"

void TextElement::Draw(DrawContext& context, Node* node)
{
	TextElement::Data* data = static_cast<TextElement::Data*>(node->data);

	if (!node->firstChild && data->text)
	{
		Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
		uint8_t textColour = 0;
		context.surface->DrawString(context, font, data->text, node->anchor.x, node->anchor.y, textColour, node->style.fontStyle);
		//Platform::video->InvertRect(node->anchor.x, node->anchor.y + 50, node->size.x, node->size.y);
		//printf("%s [%d, %d](%d %d)", data->text, node->anchor.x, node->anchor.y, node->style.fontStyle, node->style.fontSize);
	}
}

Node* TextElement::Construct(Allocator& allocator, const char* text)
{
	const char* textString = allocator.AllocString(text);
	if (textString)
	{
		TextElement::Data* data = allocator.Alloc<TextElement::Data>(textString);
		if (data)
		{
			return allocator.Alloc<Node>(Node::Text, data);
		}
	}

	return nullptr;
}

void TextElement::GenerateLayout(Layout& layout, Node* node)
{
	TextElement::Data* data = static_cast<TextElement::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);	// TODO: Get font from active style
	int lineHeight = font->glyphHeight;

	// Clear out SubTextElement children if we are regenerating the layout
	for (Node* child = node->firstChild; child; child = child->next)
	{
		TextElement::Data* childData = static_cast<TextElement::Data*>(child->data);
		if (childData->text && child != node->firstChild)
		{
			// Remove break and replace with white space
			childData->text[-1] = ' ';
		}
		childData->text = nullptr;
	}

	char* text = data->text;
	int charIndex = 0;
	int startIndex = 0;
	int lastBreakPoint = 0;
	int lastBreakPointWidth = 0;
	int width = 0;
	Node* subTextNode = node->firstChild;

	for(charIndex = 0; data->text[charIndex]; charIndex++)
	{
		char c = data->text[charIndex];

		if (c == ' ' || c == '\t')
		{
			lastBreakPoint = charIndex;
			lastBreakPointWidth = width;
		}

		width += font->GetGlyphWidth(c, node->style.fontStyle);

		if (width > layout.AvailableWidth())
		{
			// Needs a line break
			if (startIndex == lastBreakPoint)
			{
				// Nothing could fit on the line before the break
				layout.BreakNewLine();
			}
			else
			{
				if (!subTextNode)
				{
					subTextNode = SubTextElement::Construct(layout.page.allocator, &text[startIndex]);
					node->AddChild(subTextNode);
				}
				else 
				{
					TextElement::Data* childData = static_cast<TextElement::Data*>(subTextNode->data);
					childData->text = &text[startIndex];
				}

				subTextNode->anchor = layout.GetCursor(lineHeight);
				subTextNode->size.x = lastBreakPointWidth;
				subTextNode->size.y = lineHeight;

				text[lastBreakPoint] = '\0';
				startIndex = lastBreakPoint + 1;
				width -= lastBreakPointWidth;

				layout.ProgressCursor(subTextNode, lastBreakPointWidth, lineHeight);
				layout.BreakNewLine();

				subTextNode = subTextNode->next;
			}
		}
	}

	if (charIndex > startIndex)
	{
		if (startIndex == 0 && !subTextNode)
		{
			// No sub text nodes needed
			node->anchor = layout.GetCursor(lineHeight);
			node->size.x = width;
			node->size.y = lineHeight;
			layout.ProgressCursor(node, width, lineHeight);
		}
		else
		{
			if (!subTextNode)
			{
				subTextNode = SubTextElement::Construct(layout.page.allocator, &text[startIndex]);
				node->AddChild(subTextNode);
			}
			else
			{
				TextElement::Data* childData = static_cast<TextElement::Data*>(subTextNode->data);
				childData->text = &text[startIndex];
			}
			subTextNode->anchor = layout.GetCursor(lineHeight);
			subTextNode->size.x = width;
			subTextNode->size.y = lineHeight;

			layout.ProgressCursor(subTextNode, width, lineHeight);
		}
	}
}

Node* SubTextElement::Construct(Allocator& allocator, const char* text)
{
	TextElement::Data* data = allocator.Alloc<TextElement::Data>(text);
	if (data)
	{
		return allocator.Alloc<Node>(Node::SubText, data);
	}

	return nullptr;
}

void SubTextElement::GenerateLayout(Layout& layout, Node* node)
{
	TextElement::Data* data = static_cast<TextElement::Data*>(node->data);
}
