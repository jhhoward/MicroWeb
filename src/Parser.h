#pragma once

class HTMLRenderer;
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
	HTMLParser(Page& page, HTMLRenderer& renderer);

	void Parse(char* buffer, size_t count);

	Page& page;
	HTMLRenderer& renderer;

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
	char textBuffer[256];
	size_t textBufferSize;

	HTMLParseSection::Type sectionStack[MAX_PARSE_SECTION_STACK_SIZE];
	unsigned int sectionStackSize;
};
