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

#pragma once

#include <stdint.h>
class Page;
class Node;
class HTMLTagHandler;

struct TextEncoding
{
	enum Type
	{
		UTF8,
		ISO_8859_1,
		ISO_8859_2
	};
};

struct HTMLParseSection
{
	enum Type
	{
		Document,
		Head,
		Body,
		Script,
		Style,
		Title
	};
};

struct HTMLParseContext
{
	Node* node;
	const HTMLTagHandler* tag;
};

class AttributeParser
{
public:
	AttributeParser(char* inAttributeString) : attributeString(inAttributeString), key(NULL), value(NULL) {}
	bool Parse();
	
	const char* Key() { return key; }
	const char* Value() { return value; }
	
private:
	bool IsWhiteSpace(char c);
	
	char* attributeString;
	char* key;
	char* value;
};

#define MAX_PARSE_SECTION_STACK_SIZE 32
#define MAX_PARSE_CONTEXT_STACK_SIZE 32

class HTMLParser
{
public:
	HTMLParser(Page& page);

	void Reset();
	void Parse(char* buffer, size_t count);

	Page& page;

	void PushSection(HTMLParseSection::Type section);
	void PopSection(HTMLParseSection::Type section);
	HTMLParseSection::Type CurrentSection() { return sectionStack[sectionStackSize]; }

	void PushContext(Node* node, const HTMLTagHandler* tag);
	void PopContext(const HTMLTagHandler* tag);
	HTMLParseContext& CurrentContext() { return contextStack[contextStackSize]; }

	void SetTextEncoding(TextEncoding::Type newType);

	void PushPreFormatted();
	void PopPreFormatted();

private:
	void ParseChar(char c);

	void AddTextElement(const char* text);

	//HTMLNode* CreateNode(HTMLNode::NodeType nodeType, HTMLNode* parentNode);
	void AppendTextBuffer(char c);
	void FlushTextBuffer();
	bool IsWhiteSpace(char c);

	enum ParseState
	{
		ParseText,
		ParsePossibleTag,
		ParseTag,
		ParseAmpersandEscape,
		ParseComment
	};
	
	ParseState parseState;
	char textBuffer[2560];
	size_t textBufferSize;

	HTMLParseSection::Type sectionStack[MAX_PARSE_SECTION_STACK_SIZE];
	unsigned int sectionStackSize;

	HTMLParseContext contextStack[MAX_PARSE_CONTEXT_STACK_SIZE];
	unsigned int contextStackSize;

	bool parsingUnicode;
	int unicodeByteCount;
	uint32_t unicodePoint;

	TextEncoding::Type textEncoding;

	unsigned int preformatted;
};
