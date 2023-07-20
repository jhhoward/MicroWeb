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

#ifndef _TAGS_H_
#define _TAGS_H_

#include "Parser.h"
#include "Font.h"
#include "Nodes/Section.h"
#include "Style.h"

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
	SectionTagHandler(const char* inName, SectionElement::Type inSectionType) : HTMLTagHandler(inName), sectionType(inSectionType) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const SectionElement::Type sectionType;
};

class AlignmentTagHandler : public HTMLTagHandler
{
public:
	AlignmentTagHandler(const char* inName, ElementAlignment::Type inAlignmentType) : HTMLTagHandler(inName), alignmentType(inAlignmentType) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
	const ElementAlignment::Type alignmentType;
};

class PreformattedTagHandler : public HTMLTagHandler
{
public:
	PreformattedTagHandler(const char* inName) : HTMLTagHandler(inName) {}
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

class BlockTagHandler : public HTMLTagHandler
{
public:
	BlockTagHandler(const char* inName, bool inUseVerticalPadding = true, int inLeftMarginPadding = 0) : HTMLTagHandler(inName), useVerticalPadding(inUseVerticalPadding), leftMarginPadding(inLeftMarginPadding) {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
private:
	bool useVerticalPadding;
	int leftMarginPadding;
};

class InputTagHandler : public HTMLTagHandler
{
public:
	InputTagHandler() : HTMLTagHandler("input") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class FormTagHandler : public HTMLTagHandler
{
public:
	FormTagHandler() : HTMLTagHandler("form") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
	virtual void Close(class HTMLParser& parser) const;
};

class ImgTagHandler : public HTMLTagHandler
{
public:
	ImgTagHandler() : HTMLTagHandler("img") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class MetaTagHandler : public HTMLTagHandler
{
public:
	MetaTagHandler() : HTMLTagHandler("meta") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

class ButtonTagHandler : public HTMLTagHandler
{
public:
	ButtonTagHandler() : HTMLTagHandler("button") {}
	virtual void Open(class HTMLParser& parser, char* attributeStr) const;
};

const HTMLTagHandler* DetermineTag(const char* str);

#endif
