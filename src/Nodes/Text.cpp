#include "../Platform.h"
#include "../Page.h"
#include "Text.h"
#include "../Memory/Memory.h"
#include "../Layout.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"
#include "../Interface.h"
#include "../App.h"

Node* TextElement::Construct(Allocator& allocator, const char* text)
{
	MemBlockHandle textHandle = MemoryManager::pageBlockAllocator.AllocString(text);
	if (textHandle.IsAllocated())
	{
		TextElement::Data* data = allocator.Alloc<TextElement::Data>(textHandle);
		if (data)
		{
			return allocator.Alloc<Node>(Node::Text, data);
		}
	}

	return nullptr;
}

void TextElement::Draw(DrawContext& context, Node* node)
{
	TextElement::Data* data = static_cast<TextElement::Data*>(node->data);

	if (!node->firstChild && data->text.IsAllocated())
	{
		Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

		uint8_t textColour = node->style.fontColour;
		char* text = data->text.Get<char*>();

		if (context.surface->bpp == 1)
		{
			textColour = Platform::video->colourScheme.textColour;
		}

		context.surface->DrawString(context, font, text, node->anchor.x, node->anchor.y, textColour, node->style.fontStyle);

		Node* focusedNode = App::Get().ui.GetFocusedNode();
		if (focusedNode && node->IsChildOf(focusedNode))
		{
			context.surface->InvertRect(context, node->anchor.x, node->anchor.y, node->size.x, node->size.y);
		}
	}
}


void TextElement::GenerateLayout(Layout& layout, Node* node)
{
	TextElement::Data* data = static_cast<TextElement::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	int lineHeight = font->glyphHeight;

#if 1
	// TODO: optimisation
	if (data->lastAvailableWidth != -1)
	{
		// We must be regenerating this text element, see if we can just shuffle position rather than
		// completely regenerating the whole layout
		if (data->lastAvailableWidth == layout.AvailableWidth())
		{
			if (node->firstChild)
			{
				for (Node* child = node->firstChild; child; child = child->next)
				{
					child->anchor = layout.GetCursor(lineHeight);
					layout.ProgressCursor(child, child->size.x, child->size.y);
					if (child->next)
					{
						layout.BreakNewLine();
					}
				}
			}
			else
			{
				node->anchor = layout.GetCursor(lineHeight);
				layout.ProgressCursor(node, node->size.x, node->size.y);
			}

			return;
		}
	}

	data->lastAvailableWidth = layout.AvailableWidth();
#endif
	

	// Clear out SubTextElement children if we are regenerating the layout
	for (Node* child = node->firstChild; child; child = child->next)
	{
		SubTextElement::Data* childData = static_cast<SubTextElement::Data*>(child->data);
		childData->startIndex = 0;
		childData->length = 0;
		child->anchor = layout.GetCursor();
		child->size.Clear();
	}

	node->size.Clear();

	char* text = data->text.Get<char*>();
	int charIndex = 0;
	int startIndex = 0;
	int lastBreakPoint = 0;
	int lastBreakPointWidth = 0;
	int width = 0;
	Node* subTextNode = node->firstChild;
	bool hasModified = false;

	for(charIndex = 0; ; charIndex++)
	{
		char c = text[charIndex];
		bool isEnd = text[charIndex + 1] == 0;

		if (c == ' ' || c == '\t')
		{
			lastBreakPoint = charIndex;
			lastBreakPointWidth = width;
		}

		if (c == '\x1f')
		{
			// Non breaking space
			text[charIndex] = ' ';
			hasModified = true;
		}

		int glyphWidth = font->GetGlyphWidth(c, node->style.fontStyle);
		width += glyphWidth;

		bool cannotFit = width > layout.AvailableWidth();

		if (cannotFit && !lastBreakPoint && layout.AvailableWidth() < layout.MaxAvailableWidth())
		{
			// Nothing could fit on the line before the break, just add a line break
			layout.BreakNewLine();
			cannotFit = width > layout.AvailableWidth();
		}

		if (cannotFit || isEnd)
		{
			// Needs a line break

			int emitStartPosition = startIndex;
			int emitLength;
			int emitWidth;
			int nextIndex;

			if (isEnd && !cannotFit)
			{
				// End of the line so just emit everything
				emitLength = charIndex + 1 - emitStartPosition;
				emitWidth = width;
				nextIndex = -1;

				if (!node->firstChild)
				{
					// No line breaks so don't create sub text nodes
					node->anchor = layout.GetCursor(lineHeight);
					node->size.x = emitWidth;
					node->size.y = lineHeight;

					layout.ProgressCursor(node, emitWidth, lineHeight);
					break;
				}
			}
			else if (lastBreakPoint)
			{
				// There was a space or tab that we can break at
				emitLength = lastBreakPoint - emitStartPosition;
				emitWidth = lastBreakPointWidth;
				nextIndex = lastBreakPoint + 1;
			}
			else
			{
				// Need to break in the middle of a word
				emitLength = charIndex - emitStartPosition;
				emitWidth = width - glyphWidth;
				nextIndex = charIndex;
			}
			
			if (!subTextNode)
			{
				subTextNode = SubTextElement::Construct(MemoryManager::pageAllocator, emitStartPosition, emitLength);
				node->AddChild(subTextNode);
			}
			else
			{
				SubTextElement::Data* subTextData = static_cast<SubTextElement::Data*>(subTextNode->data);
				subTextData->startIndex = emitStartPosition;
				subTextData->length = emitLength;
			}

			subTextNode->anchor = layout.GetCursor(lineHeight);
			subTextNode->size.x = emitWidth;
			subTextNode->size.y = lineHeight;

			startIndex = nextIndex;
			width -= emitWidth;

			layout.ProgressCursor(subTextNode, emitWidth, lineHeight);
			subTextNode = subTextNode->next;

			if (isEnd)
			{
				break;
			}

			lastBreakPoint = 0;
			lastBreakPointWidth = 0;

			layout.BreakNewLine();
		}
	}

	if (hasModified)
	{
		data->text.Commit();
	}
}

Node* SubTextElement::Construct(Allocator& allocator, int startIndex, int length)
{
	SubTextElement::Data* data = allocator.Alloc<SubTextElement::Data>(startIndex, length);
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

void SubTextElement::Draw(DrawContext& context, Node* node)
{
	TextElement::Data* textData = static_cast<TextElement::Data*>(node->parent->data);
	SubTextElement::Data* subTextData = static_cast<SubTextElement::Data*>(node->data);

	if (textData && subTextData && textData->text.IsAllocated())
	{
		Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
		uint8_t textColour = node->style.fontColour;
		char* text = textData->text.Get<char*>() + subTextData->startIndex;
		char temp = text[subTextData->length];
		text[subTextData->length] = 0;

		if (context.surface->bpp == 1)
		{
			textColour = Platform::video->colourScheme.textColour;
		}

		context.surface->DrawString(context, font, text, node->anchor.x, node->anchor.y, textColour, node->style.fontStyle);

		Node* focusedNode = App::Get().ui.GetFocusedNode();
		if (focusedNode && node->IsChildOf(focusedNode))
		{
			context.surface->InvertRect(context, node->anchor.x, node->anchor.y, node->size.x, node->size.y);
		}

		text[subTextData->length] = temp;
		//printf("%s [%d, %d](%d %d)", data->text, node->anchor.x, node->anchor.y, node->style.fontStyle, node->style.fontSize);
	}
}
