//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "Parser.h"
#include "Tags.h"
#include "Page.h"
#include "Unicode.inc"
#include "Nodes/Text.h"
#include "Nodes/ImgNode.h"

// debug
#include "Platform.h"
#include "Draw/Surface.h"
#include "App.h"

HTMLParser::HTMLParser(Page& inPage)
: page(inPage)
, currentParseSection(SectionElement::Document)
, contextStackSize(0)
, parseState(ParseText)
, textBufferSize(0)
, parsingUnicode(false)
, preformatted(0)
{
}

void HTMLParser::Reset()
{
	parseState = ParseText;
	textBufferSize = 0;
	currentParseSection = SectionElement::Document;
	preformatted = 0;
	SetTextEncoding(TextEncoding::UTF8);

	contextStackSize = -1;
	PushContext(page.GetRootNode(), nullptr);
}

void HTMLParser::PushContext(Node* node, const HTMLTagHandler* tag)
{
	if (!node)
	{
		return;
	}
	if (contextStackSize < MAX_PARSE_CONTEXT_STACK_SIZE - 1)
	{
		if (contextStackSize >= 0)
		{
			Node* parentNode = CurrentContext().node;
			parentNode->AddChild(node);
			node->Handler().ApplyStyle(node);
		}

		contextStackSize++;
		contextStack[contextStackSize].node = node;
		contextStack[contextStackSize].tag = tag;

		if (node->type == Node::Section)
		{
			SectionElement::Data* data = static_cast<SectionElement::Data*>(node->data);
			currentParseSection = data->type;
		}

		node->Handler().BeginLayoutContext(page.layout, node);
	}
	else
	{
		// TODO: ERROR
	}
}

void HTMLParser::PopContext(const HTMLTagHandler* tag)
{
	for (int n = contextStackSize; n >= 0; n--)
	{
		HTMLParseContext& parseContext = contextStack[n];

		parseContext.node->Handler().EndLayoutContext(page.layout, parseContext.node);

		// Search for matching tag
		if (parseContext.tag == tag)
		{
			contextStackSize = n - 1;

			if (contextStackSize == 0)
			{
				page.GetRootNode()->EncapsulateChildren();
				DrawContext context;
				page.GetApp().pageRenderer.GenerateDrawContext(context, page.GetRootNode());
				
#ifdef _WIN32
				page.DebugDumpNodeGraph(page.GetRootNode());
#endif
				page.DebugDraw(context, page.GetRootNode());
			}

			// We may have exited a section so update
			for (int i = contextStackSize; i >= 0; i--)
			{
				if (contextStack[i].node->type == Node::Section)
				{
					SectionElement::Data* data = static_cast<SectionElement::Data*>(contextStack[i].node->data);
					currentParseSection = data->type;
					break;
				}
			}

			return;
		}
	}

	// TODO: ERROR
}

#define NUM_AMPERSAND_ESCAPE_SEQUENCES 14
const char* ampersandEscapeSequences[14 * 2] = 
{
	"quot",	"\"",
	"amp",	"&",
	"lt",	"<",
	"gt",	">",
	"nbsp",	" ",
	"pound",	"£",
	"brvbar",	"¦",
	"uml",	"\"",
	"not", "¬",
	"cent", "c",
	"copy", "(C)",
	"reg", "(R)",
	"laquo", "<<",
	"raquo", ">>",
};

void HTMLParser::AppendTextBuffer(char c)
{
	if(textBufferSize == sizeof(textBuffer) - 1)
	{
		FlushTextBuffer();
	}
	if(textBufferSize < sizeof(textBuffer) - 1)
	{
		textBuffer[textBufferSize] = c;
		textBufferSize++;
	}
}

void HTMLParser::EmitText(const char* text)
{
	Node* textElement = TextElement::Construct(page.allocator, text);
	if (textElement)
	{
		textElement->style = CurrentContext().node->style;
		EmitNode(textElement);
	}
}

void HTMLParser::EmitNode(Node* node)
{
	if (!node)
	{
		return;
	}

	Node* parentNode = CurrentContext().node;
	parentNode->AddChild(node);

	node->Handler().ApplyStyle(node);

	page.layout.OnNodeEmitted(node);
}

void HTMLParser::EmitImage(Image* image, int imageWidth, int imageHeight)
{
	Node* node = ImageNode::Construct(page.allocator, image);
	if (node)
	{
		node->size.x = imageWidth;
		node->size.y = imageHeight;
		EmitNode(node);
	}
}


void HTMLParser::FlushTextBuffer()
{
	textBuffer[textBufferSize] = '\0';
	
	switch(parseState)
	{
		case ParseText:
		{
			if(CurrentSection() == SectionElement::Body && textBufferSize > 0)
			{
				EmitText(textBuffer);
			}
			if (CurrentSection() == SectionElement::Title && textBufferSize > 0)
			{
				page.SetTitle(textBuffer);
			}
		}
		break;
		
		case ParseTag:
		{
			if (textBufferSize > 0)
			{
				char* tagStr = textBuffer;
				bool isCloseTag = false;

				if (tagStr[0] == '/')
				{
					isCloseTag = true;
					tagStr++;
					while (IsWhiteSpace(*tagStr))
					{
						tagStr++;
					}
				}

				char* attributeStr = tagStr;
				while (*attributeStr && !IsWhiteSpace(*attributeStr))
				{
					attributeStr++;
				}
				if (*attributeStr)
				{
					*attributeStr = '\0';
					attributeStr++;
				}

				const HTMLTagHandler* tagHandler = DetermineTag(tagStr);

				// Special case when parsing <script> tag : just ignore all other tags until we have a closing </script>
				if (CurrentSection() == SectionElement::Script)
				{
					if (!isCloseTag || stricmp(tagHandler->name, "script"))
					{
						break;
					}
				}

				if (isCloseTag)
				{
					tagHandler->Close(*this);
				}
				else
				{
					tagHandler->Open(*this, attributeStr);
				}
			}
		}		
		break;
		case ParseAmpersandEscape:
		{
			if(textBufferSize == 0)
			{
				EmitText("&");
			}
			else
			{
				for(int n = 0; n < NUM_AMPERSAND_ESCAPE_SEQUENCES; n++)
				{
					const char* escapeSequence = ampersandEscapeSequences[n * 2];
					bool matching = true;
					
					for(int i = 0; i < textBufferSize; i++)
					{
						if(escapeSequence[i] == '\0')
						{
							matching = false;
							break;
						}
						if(tolower(textBuffer[i]) != escapeSequence[i])
						{
							matching = false;
							break;
						}
					}
					
					if(matching && escapeSequence[textBufferSize] == '\0')
					{
						EmitText(ampersandEscapeSequences[n * 2 + 1]);
						break;
					}
				}
			}
		}
		break;
	}
	
	textBufferSize = 0;
	textBuffer[0] = '\0';
}

bool HTMLParser::IsWhiteSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

void HTMLParser::Parse(char* buffer, size_t count)
{
	while (count && page.allocator.GetError() == LinearAllocator::Error_None)
	{
		char c = *buffer++;
		count--;

		if ((unsigned char)c > 127)
		{
			switch (textEncoding)
			{
			case TextEncoding::UTF8:
				if (!parsingUnicode)
				{
					parsingUnicode = true;
					unicodePoint = 0;

					unsigned char code = (unsigned char)(c);
					if ((code & 0xe0) == 0xc0)		// Starts with 110
					{
						unicodeByteCount = 2;
						unicodePoint = (code & 0x1f);
					}
					else if ((code & 0xf0) == 0xe0)		// Starts with 1110
					{
						unicodeByteCount = 3;
						unicodePoint = (code & 0xf);
					}
					else if ((code & 0xf8) == 0xf0)		// Starts with 11110
					{
						unicodeByteCount = 4;
						unicodePoint = (code & 0x7);
					}
					else
					{
						// Invalid
						parsingUnicode = false;
					}

					if (parsingUnicode)
					{
						unicodeByteCount--;
						unicodePoint <<= 6;
					}
				}
				else
				{
					unsigned char code = (unsigned char)(c);

					unicodePoint |= (code & 0x3f);
					unicodeByteCount--;

					if (unicodeByteCount)
					{
						unicodePoint <<= 6;
					}
					else
					{
						// Unicode point is complete
						parsingUnicode = false;
						TextEncodingPage* encodingPage = NULL;

						if (unicodePoint >= 0x80 && unicodePoint <= 0xff)
						{
							encodingPage = &UTF8_Latin1Supplement;
							unicodePoint -= 0x80;
						}
						else if (unicodePoint >= 0x100 && unicodePoint <= 0x17f)
						{
							encodingPage = &UTF8_LatinExtendedA;
							unicodePoint -= 0x100;
						}
						if (encodingPage)
						{
							for (const char* s = encodingPage->replacement[unicodePoint]; s && *s; s++)
							{
								ParseChar(*s);
							}
						}
						else
						{
							ParseChar('?');
						}
					}
				}
				break;

			case TextEncoding::ISO_8859_1:
				for (const char* s = ISO_8859_1_Encoding.replacement[(unsigned char)c - 128]; s && *s; s++)
				{
					ParseChar(*s);
				}
				break;

			case TextEncoding::ISO_8859_2:
				for (const char* s = ISO_8859_2_Encoding.replacement[(unsigned char)c - 128]; s && *s; s++)
				{
					ParseChar(*s);
				}
				break;
			}
		}
		else
		{
			parsingUnicode = false;
			ParseChar(c);
		}
	}
}

void HTMLParser::SetTextEncoding(TextEncoding::Type newType)
{
	textEncoding = newType;
	parsingUnicode = false;
}

void HTMLParser::PushPreFormatted()
{
	preformatted++;
}

void HTMLParser::PopPreFormatted()
{
	if (preformatted > 0)
	{
		preformatted--;
	}
}

void HTMLParser::ParseChar(char c)
{
	switch(parseState)
	{
		case ParseText:
		if(c == '<')
		{
			parseState = ParsePossibleTag;
		}
		else if(c == '&')
		{
			FlushTextBuffer();
			parseState = ParseAmpersandEscape;
		}
		else
		{
			if (!preformatted)
			{
				if (IsWhiteSpace(c))
				{
					c = ' ';
					if (textBufferSize == 0)
					{
						// TODO-refactor
						//page.FlagLeadingWhiteSpace();
						break;
					}
					else
					{
						if (IsWhiteSpace(textBuffer[textBufferSize - 1]))
						{
							break;
						}
					}
				}
			}
			else 
			{
				if (c == '\n')
				{
					FlushTextBuffer();
					
					// TODO-refactor
					//page.BreakTextLine();
					break;
				}
				else if (c == '\r')
				{
					break;
				}
			}

			AppendTextBuffer(c);
		}
		break;
			
		case ParsePossibleTag:
		parseState = ParseText;
		if(IsWhiteSpace(c))
		{
			AppendTextBuffer('<');
			AppendTextBuffer(' ');
		}
		else
		{
			FlushTextBuffer();
			parseState = ParseTag;
			AppendTextBuffer(c);
		}
		break;
			
		case ParseTag:
		if(c == '>')
		{
			FlushTextBuffer();
			parseState = ParseText;
		}
		else
		{
			AppendTextBuffer(c);

			if (textBufferSize == 3 && textBuffer[0] == '!' && textBuffer[1] == '-' && textBuffer[2] == '-')
			{
				parseState = ParseComment;
				textBufferSize = 0;
			}

			// Special case when parsing script tags : we just want to look for a script closing tag
			if (CurrentSection() == SectionElement::Script && textBufferSize >= 7 && strnicmp(textBuffer, "/script", 7))
			{
				textBufferSize = 0;
				textBuffer[0] = '\0';
				parseState = ParseText;
			}
		}
			
		break;
			
		case ParseAmpersandEscape:
		if(c == ';' || IsWhiteSpace(c))
		{
			FlushTextBuffer();
			parseState = ParseText;
			if(IsWhiteSpace(c))
			{
				AppendTextBuffer(' ');
			}
		}
		else
		{
			AppendTextBuffer(c);
		}
		break;

		case ParseComment:
		if (c == '-')
		{
			if (textBufferSize < 2)
			{
				textBuffer[textBufferSize++] = c;
			}
		}
		else if (c == '>' && textBufferSize == 2)
		{
			textBufferSize = 0;
			parseState = ParseText;
		}
		else
		{
			textBufferSize = 0;
		}
		break;
	}
	
}

bool AttributeParser::Parse()
{
	key = value = NULL;

	// Skip any white space
	while(*attributeString && IsWhiteSpace(*attributeString))
	{
		attributeString++;
	}
	if(*attributeString == '\0')
	{
		return false;
	}
	
	bool foundEquals = false;
	
	// Parse key
	if(*attributeString == '"')
	{
		attributeString++;
		key = attributeString;
		while(*attributeString != '"')
		{
			if(*attributeString == '\0')
			{
				return false;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	else if (*attributeString == '\'')
	{
		attributeString++;
		key = attributeString;
		while (*attributeString != '\'')
		{
			if (*attributeString == '\0')
			{
				return false;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	else
	{
		key = attributeString;
		while(!IsWhiteSpace(*attributeString))
		{
			if(*attributeString == '\0')
			{
				return false;
			}
			if(*attributeString == '=')
			{
				foundEquals = true;
				break;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	
	// Parse =
	while(!foundEquals)
	{
		if(*attributeString == '\0')
		{
			return false;
		}
		if(*attributeString == '=')
		{
			break;
		}
		if(!IsWhiteSpace(*attributeString))
		{
			return false;
		}
		attributeString++;
	}
	
	// Skip any white space
	while(*attributeString && IsWhiteSpace(*attributeString))
	{
		attributeString++;
	}
	if(*attributeString == '\0')
	{
		return false;
	}
	
	// Parse value
	if(*attributeString == '"')
	{
		attributeString++;
		value = attributeString;
		while(*attributeString != '"')
		{
			if(*attributeString == '\0')
			{
				return false;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	else if (*attributeString == '\'')
	{
		attributeString++;
		value = attributeString;
		while (*attributeString != '\'')
		{
			if (*attributeString == '\0')
			{
				return false;
			}
			attributeString++;
		}
		*attributeString = '\0';
		attributeString++;
	}
	else
	{
		value = attributeString;
		while(*attributeString && !IsWhiteSpace(*attributeString))
		{
			attributeString++;
		}
		
		if(*attributeString)
		{
			*attributeString = '\0';
			attributeString++;
		}
	}
			
	return true;
}

bool AttributeParser::IsWhiteSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t';
}

