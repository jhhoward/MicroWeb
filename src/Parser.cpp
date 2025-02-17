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
#include "Nodes/Button.h"
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

HTMLParseContext* HTMLParser::FindContextInStack(Node::Type nodeType)
{
	for (Stack<HTMLParseContext>::Entry* entry = contextStack.top; entry; entry = entry->prev)
	{
		if (entry->obj.node && entry->obj.node->type == nodeType)
		{
			return &entry->obj;
		}
	}
	return nullptr;
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

	//node->Handler().BeginLayoutContext(page.layout, node);
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

		//parseContext.node->Handler().EndLayoutContext(page.layout, parseContext.node);

		if (parseContext.tag == tag)
		{
			// If the stack is emptied then we have finished parsing the document
			if (contextStackSize == 0 && parseContext.parseSection == SectionElement::HTML)
			{
#ifdef WIN32
				page.DebugDumpNodeGraph();
#endif
				Finish();
				//page.GetApp().pageRenderer.RefreshAll();
				//page.GetApp().pageRenderer.MarkPageLayoutComplete();
			}

			return;
		}
	}

	Platform::FatalError("Error popping context in HTML parser");
}

void HTMLParser::Finish()
{
	FlushTextBuffer();
	parseState = ParseFinished;
	page.layout.MarkParsingComplete();
	page.GetApp().pageLoadTask.Stop();
}

#define NUM_AMPERSAND_ESCAPE_SEQUENCES (sizeof(ampersandEscapeSequences) / (2 * sizeof(const char*)))
const char* ampersandEscapeSequences[] = 
{
	"quot",			"\"",
	"amp",			"&",
	"lt",			"<",
	"gt",			">",
	"nbsp",			"\x1f",
	"laquo",		"<<",
	"raquo",		">>",
	"iexcl",		"\xA1",					//	¡	161	&iexcl;	inverted exclamation mark
	"cent",			"\xA2",					//	¢	162	&cent;	cent sign
	"pound",		"\xA3",					//	£	163	&pound;	pound sign
	"curren",		"\xA4",					//	¤	164	&curren;	currency sign
	"yen",			"\xA5",					//	¥	165	&yen;	yen sign
	"brvbar",		"\xA6",					//	¦	166	&brvbar;	broken bar
	"sect",			"\xA7",					//	§	167	&sect;	section sign
	"uml",			"\xA8",					//	¨	168	&uml;	diaeresis
	"copy",			"\xA9",					//	©	169	&copy;	copyright sign
	"ordf",			"\xAA",					//	ª	170	&ordf;	feminine ordinal indicator
	"laquo",		"\xAB",					//	«	171	&laquo;	left-pointing double angle quotation mark
	"not",			"\xAC",					//	¬	172	&not;	not sign
	"shy",			"\xAD",					//	�­	173	&shy;	soft hyphen
	"reg",			"\xAE",					//	®	174	&reg;	registered sign
	"macr",			"\xAF",					//	¯	175	&macr;	macron
	"deg",			"\xB0",					//	°	176	&deg;	degree sign
	"plusmn",		"\xB1",					//	±	177	&plusmn;	plus-minus sign
	"sup2",			"\xB2",					//	²	178	&sup2;	superscript two
	"sup3",			"\xB3",					//	³	179	&sup3;	superscript three
	"acute",		"\xB4",					//	´	180	&acute;	acute accent
	"micro",		"\xB5",					//	µ	181	&micro;	micro sign
	"para",			"\xB6",					//	¶	182	&para;	pilcrow sign
	"middot",		"\xB7",					//	·	183	&middot;	middle dot
	"cedil",		"\xB8",					//	¸	184	&cedil;	cedilla
	"sup1",			"\xB9",					//	¹	185	&sup1;	superscript one
	"ordm",			"\xBA",					//	º	186	&ordm;	masculine ordinal indicator
	"raquo",		"\xBB",					//	»	187	&raquo;	right-pointing double angle quotation mark
	"frac14",		"\xBC",					//	¼	188	&frac14;	vulgar fraction one quarter
	"frac12",		"\xBD",					//	½	189	&frac12;	vulgar fraction one half
	"frac34",		"\xBE",					//	¾	190	&frac34;	vulgar fraction three quarters
	"iquest",		"\xBF",					//	¿	191	&iquest;	inverted question mark
	"Agrave",		"\xC0",					//	À	192	&Agrave;	Latin capital letter A with grave
	"Aacute",		"\xC1",					//	Á	193	&Aacute;	Latin capital letter A with acute
	"Acirc",		"\xC2",					//	Â	194	&Acirc;	Latin capital letter A with circumflex
	"Atilde",		"\xC3",					//	Ã	195	&Atilde;	Latin capital letter A with tilde
	"Auml",			"\xC4",					//	Ä	196	&Auml;	Latin capital letter A with diaeresis
	"Aring",		"\xC5",					//	Å	197	&Aring;	Latin capital letter A with ring above
	"AElig",		"\xC6",					//	Æ	198	&AElig;	Latin capital letter AE
	"Ccedil",		"\xC7",					//	Ç	199	&Ccedil;	Latin capital letter C with cedilla
	"Egrave",		"\xC8",					//	È	200	&Egrave;	Latin capital letter E with grave
	"Eacute",		"\xC9",					//	É	201	&Eacute;	Latin capital letter E with acute
	"Ecirc",		"\xCA",					//	Ê	202	&Ecirc;	Latin capital letter E with circumflex
	"Euml",			"\xCB",					//	Ë	203	&Euml;	Latin capital letter E with diaeresis
	"Igrave",		"\xCC",					//	Ì	204	&Igrave;	Latin capital letter I with grave
	"Iacute",		"\xCD",					//	Í	205	&Iacute;	Latin capital letter I with acute
	"Icirc",		"\xCE",					//	Î	206	&Icirc;	Latin capital letter I with circumflex
	"Iuml",			"\xCF",					//	Ï	207	&Iuml;	Latin capital letter I with diaeresis
	"ETH",			"\xD0",					//	Ð	208	&ETH;	Latin capital letter Eth
	"Ntilde",		"\xD1",					//	Ñ	209	&Ntilde;	Latin capital letter N with tilde
	"Ograve",		"\xD2",					//	Ò	210	&Ograve;	Latin capital letter O with grave
	"Oacute",		"\xD3",					//	Ó	211	&Oacute;	Latin capital letter O with acute
	"Ocirc",		"\xD4",					//	Ô	212	&Ocirc;	Latin capital letter O with circumflex
	"Otilde",		"\xD5",					//	Õ	213	&Otilde;	Latin capital letter O with tilde
	"Ouml",			"\xD6",					//	Ö	214	&Ouml;	Latin capital letter O with diaeresis
	"times",		"\xD7",					//	×	215	&times;	multiplication sign
	"Oslash",		"\xD8",					//	Ø	216	&Oslash;	Latin capital letter O with stroke
	"Ugrave",		"\xD9",					//	Ù	217	&Ugrave;	Latin capital letter U with grave
	"Uacute",		"\xDA",					//	Ú	218	&Uacute;	Latin capital letter U with acute
	"Ucirc",		"\xDB",					//	Û	219	&Ucirc;	Latin capital letter U with circumflex
	"Uuml",			"\xDC",					//	Ü	220	&Uuml;	Latin capital letter U with diaeresis
	"Yacute",		"\xDD",					//	Ý	221	&Yacute;	Latin capital letter Y with acute
	"THORN",		"\xDE",					//	Þ	222	&THORN;	Latin capital letter Thorn
	"szlig",		"\xDF",					//	ß	223	&szlig;	Latin small letter sharp s
	"agrave",		"\xE0",					//	à	224	&agrave;	Latin small letter a with grave
	"aacute",		"\xE1",					//	á	225	&aacute;	Latin small letter a with acute
	"acirc",		"\xE2",					//	â	226	&acirc;	Latin small letter a with circumflex
	"atilde",		"\xE3",					//	ã	227	&atilde;	Latin small letter a with tilde
	"auml",			"\xE4",					//	ä	228	&auml;	Latin small letter a with diaeresis
	"aring",		"\xE5",					//	å	229	&aring;	Latin small letter a with ring above
	"aelig",		"\xE6",					//	æ	230	&aelig;	Latin small letter ae
	"ccedil",		"\xE7",					//	ç	231	&ccedil;	Latin small letter c with cedilla
	"egrave",		"\xE8",					//	è	232	&egrave;	Latin small letter e with grave
	"eacute",		"\xE9",					//	é	233	&eacute;	Latin small letter e with acute
	"ecirc",		"\xEA",					//	ê	234	&ecirc;	Latin small letter e with circumflex
	"euml",			"\xEB",					//	ë	235	&euml;	Latin small letter e with diaeresis
	"igrave",		"\xEC",					//	ì	236	&igrave;	Latin small letter i with grave
	"iacute",		"\xED",					//	í	237	&iacute;	Latin small letter i with acute
	"icirc",		"\xEE",					//	î	238	&icirc;	Latin small letter i with circumflex
	"iuml",			"\xEF",					//	ï	239	&iuml;	Latin small letter i with diaeresis
	"eth",			"\xF0",					//	ð	240	&eth;	Latin small letter eth
	"ntilde",		"\xF1",					//	ñ	241	&ntilde;	Latin small letter n with tilde
	"ograve",		"\xF2",					//	ò	242	&ograve;	Latin small letter o with grave
	"oacute",		"\xF3",					//	ó	243	&oacute;	Latin small letter o with acute
	"ocirc",		"\xF4",					//	ô	244	&ocirc;	Latin small letter o with circumflex
	"otilde",		"\xF5",					//	õ	245	&otilde;	Latin small letter o with tilde
	"ouml",			"\xF6",					//	ö	246	&ouml;	Latin small letter o with diaeresis
	"divide",		"\xF7",					//	÷	247	&divide;	division sign
	"oslash",		"\xF8",					//	ø	248	&oslash;	Latin small letter o with stroke
	"ugrave",		"\xF9",					//	ù	249	&ugrave;	Latin small letter u with grave
	"uacute",		"\xFA",					//	ú	250	&uacute;	Latin small letter u with acute
	"ucirc",		"\xFB",					//	û	251	&ucirc;	Latin small letter u with circumflex
	"uuml",			"\xFC",					//	ü	252	&uuml;	Latin small letter u with diaeresis
	"yacute",		"\xFD",					//	ý	253	&yacute;	Latin small letter y with acute
	"thorn",		"\xFE",					//	þ	254	&thorn;	Latin small letter thorn
	"yuml",			"\xFF",					//	ÿ	255	&yuml;	Latin small letter y with diaeresis

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
	EmitNode(TextElement::Construct(MemoryManager::pageAllocator, text));
}

void HTMLParser::EmitNode(Node* node)
{
	if (!node)
	{
		return;
	}

	// Check if this is part of a table - if so, make sure we are in a cell
	Node* tableContainer = nullptr;
	for (Node* n = CurrentContext().node; n; n = n->parent)
	{
		if (n->type == Node::Table)
		{
			tableContainer = n;
			break;
		}
	}
	if (tableContainer)
	{
		bool isInCell = false;

		for (Node* n = CurrentContext().node; n != tableContainer; n = n->parent)
		{
			if (n->type == Node::TableCell)
			{
				isInCell = true;
				break;
			}
		}

		if (!isInCell)
		{
			// Don't emit the node. In other browsers the content gets emitted before the table?
			return;
		}
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
			if(textBufferSize > 0)
			{
				HTMLParseContext* optionContext = FindContextInStack(Node::Option);
				HTMLParseContext* buttonContext = FindContextInStack(Node::Button);

				if (optionContext)
				{
					OptionNode::Data* option = static_cast<OptionNode::Data*>(optionContext->node->data);
					option->text = MemoryManager::pageAllocator.AllocString(textBuffer);
				}
				else if (buttonContext)
				{
					ButtonNode::Data* button = static_cast<ButtonNode::Data*>(buttonContext->node->data);
					button->buttonText = MemoryManager::pageAllocator.AllocString(textBuffer);
				}
				else
				{
					switch (CurrentSection())
					{
					case SectionElement::Title:
						page.SetTitle(textBuffer);
						break;
					case SectionElement::Script:
					case SectionElement::Style:
					case SectionElement::Document:
						break;
					default:
					case SectionElement::Body:
					case SectionElement::HTML:
						EmitText(textBuffer);
						break;
					}
				}
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
					HTMLParseContext& oldContext = CurrentContext();
					tagHandler->Open(*this, attributeStr);

					if (&oldContext != &CurrentContext())
					{
						AttributeParser attributes(attributeStr);
						while (attributes.Parse())
						{
							if (!stricmp(attributes.Key(), "align"))
							{
								ElementStyle style = CurrentContext().node->GetStyle();
								if (!stricmp(attributes.Value(), "center"))
								{
									style.alignment = ElementAlignment::Center;
								}
								else if (!stricmp(attributes.Value(), "left"))
								{
									style.alignment = ElementAlignment::Left;
								}
								else if (!stricmp(attributes.Value(), "right"))
								{
									style.alignment = ElementAlignment::Right;
								}
								CurrentContext().node->SetStyle(style);
							}
							if (!stricmp(attributes.Key(), "name"))
							{
								if (App::Get().ui.jumpTagName && !stricmp(attributes.Value(), App::Get().ui.jumpTagName + 1))
								{
									App::Get().ui.jumpNode = CurrentContext().node;
								}
							}
						}
					}
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

const char* HTMLParser::GetUnicodeString(int unicodePoint)
{
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
		return encodingPage->replacement[unicodePoint];
	}
	else
	{
		return "?";
	}
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

					if (number > FIRST_FONT_GLYPH && number < 128)
					{
						*buffer = (char)(number);
						strcpy(buffer + 1, nextBufferPosition);
					}
					else
					{
						const char* unicodeReplacementText = GetUnicodeString(number);
						int replacementTextLength = strlen(unicodeReplacementText);
						memcpy(buffer, unicodeReplacementText, replacementTextLength);
						strcpy(buffer + replacementTextLength, nextBufferPosition);
						buffer += replacementTextLength - 1;
					}
				}
				else
				{
					int matchingEscapeSequence = -1;

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
							if (buffer[i] != escapeSequence[i])
							{
								matching = false;
								break;
							}
						}

						if (matching && escapeSequence[escapeSequenceLength] == '\0')
						{
							matchingEscapeSequence = n;
							break;
						}
					}

					if (matchingEscapeSequence == -1)
					{
						for (int n = 0; n < NUM_AMPERSAND_ESCAPE_SEQUENCES; n++)
						{
							// Look again but this time non case sensitive comparison
							const char* escapeSequence = ampersandEscapeSequences[n * 2];
							bool matching = true;

							for (int i = 0; i < escapeSequenceLength; i++)
							{
								if (escapeSequence[i] == '\0')
								{
									matching = false;
									break;
								}
								if (tolower(buffer[i]) != tolower(escapeSequence[i]))
								{
									matching = false;
									break;
								}
							}

							if (matching && escapeSequence[escapeSequenceLength] == '\0')
							{
								matchingEscapeSequence = n;
								break;
							}
						}
					}

					if (matchingEscapeSequence != -1)
					{
						buffer--;	// One step backwards to write over the &
						const char* replacementText = ampersandEscapeSequences[matchingEscapeSequence * 2 + 1];
						int replacementTextLength = strlen(replacementText);
						memcpy(buffer, replacementText, replacementTextLength);
						strcpy(buffer + replacementTextLength, nextBufferPosition);

						if (replaceNonBreakingSpace && *buffer == '\x1f')
						{
							*buffer = ' ';
						}

						buffer += replacementTextLength - 1;
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

void HTMLParser::Write(const char* str)
{
	Parse((char*) str, strlen(str));
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

						const char* unicodeReplacement = GetUnicodeString(unicodePoint);
						for (const char* s = unicodeReplacement; s && *s; s++)
						{
							ParseChar(*s);
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

// Making this static as we only ever have one attribute parser running at a time
// and we don't want to blow the stack with a huge buffer
char AttributeParser::attributeStringBuffer[MAX_ATTRIBUTE_STRING_LENGTH];

AttributeParser::AttributeParser(const char* inAttributeString) :key(NULL), value(NULL) 
{
	strncpy(attributeStringBuffer, inAttributeString, MAX_ATTRIBUTE_STRING_LENGTH);
	attributeStringBuffer[MAX_ATTRIBUTE_STRING_LENGTH - 1] = '\0';
	attributeString = attributeStringBuffer;
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
				// Key but no value
				value = attributeString;
				return true;
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
			// Key but no value
			value = attributeString;
			return true;
		}
		if(*attributeString == '=')
		{
			break;
		}
		if(!IsWhiteSpace(*attributeString))
		{
			// Key but no value
			value = key + strlen(key);
			return true;
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
	{ "grey",	RGB332(0x80, 0x80, 0x80) },
	//	{ "silver", RGB332(0xc0, 0xc0, 0xc0) },
	{ "silver", RGB332(0xa0, 0xa0, 0xa0) },
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

		if (Platform::video->paletteLUT)
		{
			return Platform::video->paletteLUT[RGB332(red, green, blue)];
		}

		int grey = red + green + blue;
		return grey > (127 * 3) ? 1 : 0;
	}
	else
	{
		for (int n = 0; n < NUM_NAMED_COLOURS; n++)
		{
			if (!stricmp(namedColours[n].name, colourCode))
			{
				uint8_t rgb332 = namedColours[n].colour;
				if (Platform::video->paletteLUT)
				{
					return Platform::video->paletteLUT[rgb332];
				}
				else
				{
					return (rgb332 & 0xda) != 0;
				}
			}
		}
	}
	return 0;
}

