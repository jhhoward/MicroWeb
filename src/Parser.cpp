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
#include "Nodes/Break.h"
#include "Nodes/Select.h"
#include "Memory/Memory.h"
#include "App.h"

// debug
#include "Platform.h"
#include "Draw/Surface.h"
#include "App.h"

HTMLParser::HTMLParser(Page& inPage)
: page(inPage)
, contextStack(MemoryManager::pageAllocator)
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
	preformatted = 0;
	SetTextEncoding(TextEncoding::UTF8);

	contextStack.Reset();
	contextStackSize = -1;
	PushContext(page.GetRootNode(), nullptr);
}

void HTMLParser::PushContext(Node* node, const HTMLTagHandler* tag)
{
	if (!node)
	{
		return;
	}

	if (contextStackSize >= 0)
	{
		Node* parentNode = CurrentContext().node;
		parentNode->AddChild(node);
		node->Handler().ApplyStyle(node);
	}

	contextStack.Push();
	contextStackSize++;

	contextStack.Top().node = node;
	contextStack.Top().tag = tag;

	if (node->type == Node::Section)
	{
		SectionElement::Data* data = static_cast<SectionElement::Data*>(node->data);
		contextStack.Top().parseSection = data->type;
	}

	node->Handler().BeginLayoutContext(page.layout, node);
}

void HTMLParser::PopContext(const HTMLTagHandler* tag)
{
	if (contextStackSize >= 0)
	{
		bool hasEntry = false;

		// Check that the context stack has this tag (in case of malformed HTML)
		for (Stack<HTMLParseContext>::Entry* entry = contextStack.top; entry; entry = entry->prev)
		{
			if (entry->obj.tag == tag)
			{
				hasEntry = true;
				break;
			}
		}

		if (!hasEntry)
		{
			return;
		}
	}

	// Keep popping contexts until we get to the matching tag
	while(contextStackSize >= 0)
	{
		HTMLParseContext& parseContext = contextStack.Top();
		contextStack.Pop();
		contextStackSize--;

		parseContext.node->Handler().EndLayoutContext(page.layout, parseContext.node);

		if (parseContext.tag == tag)
		{
			// If the stack is emptied then we have finished parsing the document
			if (contextStackSize == 0)
			{
#ifdef _WIN32
				page.DebugDumpNodeGraph();
#endif
				//page.GetApp().pageRenderer.RefreshAll();
				page.GetApp().pageRenderer.MarkPageLayoutComplete();

				page.GetApp().StopLoad();
			}

			return;
		}
	}

	// TODO: ERROR
	exit(1);
}

#define NUM_AMPERSAND_ESCAPE_SEQUENCES (sizeof(ampersandEscapeSequences) / (2 * sizeof(const char*)))
const char* ampersandEscapeSequences[] = 
{
	"quot",	"\"",
	"amp",	"&",
	"lt",	"<",
	"gt",	">",
	"nbsp",	"\x1f",
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
	Node* textElement = TextElement::Construct(MemoryManager::pageAllocator, text);
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
	Node* node = ImageNode::Construct(MemoryManager::pageAllocator);
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
			if (CurrentContext().node && CurrentContext().node->type == Node::Option)
			{
				OptionNode::Data* option = static_cast<OptionNode::Data*>(CurrentContext().node->data);
				option->text = MemoryManager::pageAllocator.AllocString(textBuffer);
			}
			else if(CurrentSection() == SectionElement::Body && textBufferSize > 0)
			{
				EmitText(textBuffer);
			}
			else if (CurrentSection() == SectionElement::Title && textBufferSize > 0)
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
			// Flush everything before the escape sequence started
			if (escapeSequenceStartIndex)
			{
				textBufferSize = escapeSequenceStartIndex;
				parseState = ParseText;
				FlushTextBuffer();
				parseState = ParseAmpersandEscape;
				textBuffer[escapeSequenceStartIndex] = '&';
				strcpy(textBuffer, textBuffer + escapeSequenceStartIndex);
			}
			return;
		}
		break;
	}
	
	textBufferSize = 0;
	textBuffer[0] = '\0';
}

void HTMLParser::ReplaceAmpersandEscapeSequences(char* buffer, bool replaceNonBreakingSpace)
{
	while (*buffer)
	{
		if (*buffer == '&')
		{
			buffer++;

			// Find length of sequence
			int escapeSequenceLength = 0;
			while (buffer[escapeSequenceLength] && buffer[escapeSequenceLength] != ';' && !IsWhiteSpace(buffer[escapeSequenceLength]))
			{
				escapeSequenceLength++;
			}
			bool correctlyTerminated = buffer[escapeSequenceLength] == ';';
			char* nextBufferPosition = correctlyTerminated ? buffer + escapeSequenceLength + 1 : buffer + escapeSequenceLength;

			if(escapeSequenceLength > 0)
			{
				if (*buffer == '#')
				{
					int number = 0;

					// This is a entity number
					if (buffer[1] == 'x' || buffer[1] == 'X')
					{
						// Hex number
						number = strtol(buffer + 2, NULL, 16);
					}
					else
					{
						number = atoi(buffer + 1);
					}
					buffer--;
					if (number > FIRST_FONT_GLYPH && number <= LAST_FONT_GLYPH)
					{
						*buffer = (char)(number);
						strcpy(buffer + 1, nextBufferPosition);
					}
					else
					{
						strcpy(buffer, nextBufferPosition);
					}
				}
				else
				{
					for (int n = 0; n < NUM_AMPERSAND_ESCAPE_SEQUENCES; n++)
					{
						const char* escapeSequence = ampersandEscapeSequences[n * 2];
						bool matching = true;

						for (int i = 0; i < escapeSequenceLength; i++)
						{
							if (escapeSequence[i] == '\0')
							{
								matching = false;
								break;
							}
							if (tolower(buffer[i]) != escapeSequence[i])
							{
								matching = false;
								break;
							}
						}

						if (matching && escapeSequence[escapeSequenceLength] == '\0')
						{
							buffer--;	// One step backwards to write over the &
							const char* replacementText = ampersandEscapeSequences[n * 2 + 1];
							int replacementTextLength = strlen(replacementText);
							memcpy(buffer, replacementText, replacementTextLength);
							strcpy(buffer + replacementTextLength, nextBufferPosition);

							if (replaceNonBreakingSpace && *buffer == '\x1f')
							{
								*buffer = ' ';
							}

							buffer += replacementTextLength - 1;
							break;
						}
					}
				}
			}
		}
		buffer++;
	}
}

bool HTMLParser::IsWhiteSpace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

void HTMLParser::Parse(char* buffer, size_t count)
{
	while (count && MemoryManager::pageAllocator.GetError() == LinearAllocator::Error_None)
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

	if (MemoryManager::pageAllocator.GetError() && contextStackSize >= 0)
	{
		// There was a memory error, possibly out of memory. Unwind the context stack
		while (contextStackSize >= 0)
		{
			HTMLParseContext& parseContext = contextStack.Top();
			PopContext(parseContext.tag);
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
			parseState = ParseAmpersandEscape;
			escapeSequenceStartIndex = textBufferSize;
			AppendTextBuffer(c);
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
					
					EmitNode(BreakNode::Construct(MemoryManager::pageAllocator));
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
		AppendTextBuffer(c);
		if(c == ';' || IsWhiteSpace(c))
		{
			textBuffer[textBufferSize] = '\0';
			ReplaceAmpersandEscapeSequences(textBuffer + escapeSequenceStartIndex, false);
			textBufferSize = strlen(textBuffer + escapeSequenceStartIndex) + escapeSequenceStartIndex;

			parseState = ParseText;
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

NamedColour namedColours[] =
{
	{ "black",	RGB332(0x00, 0x00, 0x00) },
	{ "white",	RGB332(0xff, 0xff, 0xff) },
	{ "gray",	RGB332(0x80, 0x80, 0x80) },
	{ "silver", RGB332(0xc0, 0xc0, 0xc0) },
	{ "red",	RGB332(0xff, 0x00, 0x00) },
	{ "maroon", RGB332(0x80, 0x00, 0x00) },
	{ "yellow", RGB332(0xff, 0xff, 0x00) },
	{ "olive",	RGB332(0x80, 0x80, 0x00) },
	{ "lime",	RGB332(0x00, 0xff, 0x00) },
	{ "green",	RGB332(0x00, 0x80, 0x00) },
	{ "aqua",	RGB332(0x00, 0xff, 0xff) },
	{ "teal",	RGB332(0x00, 0x80, 0x80) },
	{ "blue",	RGB332(0x00, 0x00, 0xff) },
	{ "navy",	RGB332(0x00, 0x00, 0x80) },
	{ "fuchsia",RGB332(0xff, 0x00, 0xff) },
	{ "purple",	RGB332(0x80, 0x00, 0x80) },
	{ "orange",	RGB332(0xff, 0xa5, 0x00) }
};

#define NUM_NAMED_COLOURS (sizeof(namedColours) / sizeof(NamedColour))

uint8_t HTMLParser::ParseColourCode(const char* colourCode)
{
	if (!Platform::video->paletteLUT)
	{
		return 0;
	}

	if (colourCode[0] == '#')
	{
		uint8_t red = 0, green = 0, blue = 0;
		int codeLength = strlen(colourCode + 1);
		if (codeLength == 6 || codeLength == 8)
		{
			sscanf(colourCode, "#%02hhx%02hhx%02hhx", &red, &green, &blue);
		}
		else if (codeLength == 3 || codeLength == 4)
		{
			sscanf(colourCode, "#%1hhx%1hhx%1hhx", &red, &green, &blue);
			red += red * 16;
			green += green * 16;
			blue += blue * 16;
		}

		return Platform::video->paletteLUT[RGB332(red, green, blue)];
	}
	else
	{
		for (int n = 0; n < NUM_NAMED_COLOURS; n++)
		{
			if (!stricmp(namedColours[n].name, colourCode))
			{
				return Platform::video->paletteLUT[namedColours[n].colour];
			}
		}
	}
	return 0;
}
