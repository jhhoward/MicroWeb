#pragma once

#include "Parser.h"
#include "Font.h"

class HTMLParser;

class HTMLTagHandler
{
public:
	HTMLTagHandler(const char* inName) : name(inName) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const {}
	virtual void Close(class HTMLParser& parser) const {}
	
	const char* name;
};

class SectionTagHandler : public HTMLTagHandler
{
public:
	SectionTagHandler(const char* inName, HTMLParseSection::Type inSection) : HTMLTagHandler(inName), section(inSection) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const HTMLParseSection::Type section;
};

class CenterTagHandler : public HTMLTagHandler
{
public:
	CenterTagHandler() : HTMLTagHandler("center") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class FontTagHandler : public HTMLTagHandler
{
public:
	FontTagHandler() : HTMLTagHandler("font") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class BrTagHandler : public HTMLTagHandler
{
public:
	BrTagHandler() : HTMLTagHandler("br") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class HrTagHandler : public HTMLTagHandler
{
public:
	HrTagHandler() : HTMLTagHandler("hr") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class HTagHandler : public HTMLTagHandler
{
public:
	HTagHandler(const char* inName, int inSize) : HTMLTagHandler(inName), size(inSize) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const int size;
};

class SizeTagHandler : public HTMLTagHandler
{
public:
	SizeTagHandler(const char* inName, int inSize) : HTMLTagHandler(inName), size(inSize) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const int size;
};

class StyleTagHandler : public HTMLTagHandler
{
public:
	StyleTagHandler(const char* inName, FontStyle::Type inStyle) : HTMLTagHandler(inName), style(inStyle) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const FontStyle::Type style;
};

class ListTagHandler : public HTMLTagHandler
{
public:
	ListTagHandler(const char* inName) : HTMLTagHandler(inName) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class LiTagHandler : public HTMLTagHandler
{
public:
	LiTagHandler() : HTMLTagHandler("li") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class ATagHandler : public HTMLTagHandler
{
public:
	ATagHandler() : HTMLTagHandler("a") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class PTagHandler : public HTMLTagHandler
{
public:
	PTagHandler() : HTMLTagHandler("p") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class DivTagHandler : public HTMLTagHandler
{
public:
	DivTagHandler() : HTMLTagHandler("div") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

const HTMLTagHandler* DetermineTag(const char* str);
