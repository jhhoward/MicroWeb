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

class Page;

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

private:
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
};
