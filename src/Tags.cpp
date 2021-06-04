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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "Tags.h"
#include "Parser.h"
#include "Page.h"
#include "Platform.h"

static const HTMLTagHandler* tagHandlers[] =
{
	new HTMLTagHandler("generic"),
	new SectionTagHandler("html", HTMLParseSection::Document),
	new SectionTagHandler("head", HTMLParseSection::Head),
	new SectionTagHandler("body", HTMLParseSection::Body),
	new SectionTagHandler("script", HTMLParseSection::Script),
	new SectionTagHandler("style", HTMLParseSection::Style),
	new SectionTagHandler("title", HTMLParseSection::Title),
	new HTagHandler("h1", 1),
	new HTagHandler("h2", 2),
	new HTagHandler("h3", 3),
	new HTagHandler("h4", 4),
	new HTagHandler("h5", 5),
	new HTagHandler("h6", 6),
	new PTagHandler(),
	new BrTagHandler(),
	new CenterTagHandler(),
	new FontTagHandler(),
	new StyleTagHandler("b", FontStyle::Bold),
	new StyleTagHandler("strong", FontStyle::Bold),
	new StyleTagHandler("i", FontStyle::Italic),
	new StyleTagHandler("em", FontStyle::Italic),
	new StyleTagHandler("cite", FontStyle::Italic),
	new StyleTagHandler("var", FontStyle::Italic),
	new StyleTagHandler("u", FontStyle::Underline),
	new ATagHandler(),
	new HTMLTagHandler("img"),
	new ListTagHandler("ul"),
	new ListTagHandler("ol"),
	new ListTagHandler("menu"),
	new LiTagHandler(),
	new HrTagHandler(),
	new DivTagHandler(),
	new SizeTagHandler("small", 0),
	new InputTagHandler(),
	new FormTagHandler(),
	NULL
};


const HTMLTagHandler* DetermineTag(const char* str)
{
	for(int n = 0; ; n++)
	{
		const HTMLTagHandler* handler = tagHandlers[n];
		if (!handler)
		{
			break;
		}
		if (!stricmp(str, handler->name))
		{
			return handler;
		}
	}
	
	static const HTMLTagHandler genericTag("generic");
	return &genericTag;
}


void HrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.page.AddHorizontalRule();
}

void BrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.page.BreakTextLine();
}

void HTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	currentStyle.fontSize = size >= 3 ? 1 : 2;
	currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | FontStyle::Bold);
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
	parser.page.PushStyle(currentStyle);
}

void HTagHandler::Close(class HTMLParser& parser) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
	parser.page.PopStyle();
}

void SizeTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	currentStyle.fontSize = size;
	parser.page.PushStyle(currentStyle);
}

void SizeTagHandler::Close(class HTMLParser& parser) const
{
	parser.page.PopStyle();
}

void LiTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.page.BreakLine();
	parser.page.AppendText(" * ");
}
void LiTagHandler::Close(class HTMLParser& parser) const
{
	parser.page.BreakLine();
}

void ATagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	while(attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "href"))
		{
			parser.page.SetWidgetURL(attributes.Value());
		}
	}

	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | FontStyle::Underline);
	parser.page.PushStyle(currentStyle);

}
void ATagHandler::Close(class HTMLParser& parser) const
{
	parser.page.ClearWidgetURL();
	parser.page.PopStyle();
}

void PTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
}
void PTagHandler::Close(class HTMLParser& parser) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
}

void DivTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
}

void DivTagHandler::Close(class HTMLParser& parser) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
}

void SectionTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushSection(section);
}
void SectionTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopSection(section);
}

void StyleTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | style);
	parser.page.PushStyle(currentStyle);
}
void StyleTagHandler::Close(class HTMLParser& parser) const
{
	parser.page.PopStyle();
}

void CenterTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();

	parser.page.BreakLine();

	currentStyle.center = true;
	parser.page.PushStyle(currentStyle);
}

void CenterTagHandler::Close(class HTMLParser& parser) const
{
	parser.page.PopStyle();
	parser.page.BreakLine();
}

void FontTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();

	AttributeParser attributes(attributeStr);
	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "size"))
		{
			int size = atoi(attributes.Value());
			switch (size)
			{
			case 1:
			case 2:
				currentStyle.fontSize = 0;
				break;
			case 0:
				// Probably invalid
			case 3:
			case 4:
				currentStyle.fontSize = 1;
				break;
			default:
				// Anything bigger
				currentStyle.fontSize = 2;
				break;
			}
		}
	}

	parser.page.PushStyle(currentStyle);
}

void FontTagHandler::Close(class HTMLParser& parser) const
{
	parser.page.PopStyle();
}

void ListTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
}

void ListTagHandler::Close(class HTMLParser& parser) const
{
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
}

struct HTMLInputTag
{
	enum Type
	{
		Unknown,
		Submit,
		Text
	};
};

void InputTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	char* value = NULL;
	char* name = NULL;
	HTMLInputTag::Type type = HTMLInputTag::Unknown;
	int bufferLength = 80;

	AttributeParser attributes(attributeStr);
	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "type"))
		{
			if (!stricmp(attributes.Value(), "submit"))
			{
				type = HTMLInputTag::Submit;
			}
			else if (!stricmp(attributes.Value(), "text"))
			{
				type = HTMLInputTag::Text;
			}
		}
		if (!stricmp(attributes.Key(), "value"))
		{
			value = parser.page.allocator.AllocString(attributes.Value());
		}
		if (!stricmp(attributes.Key(), "name"))
		{
			name = parser.page.allocator.AllocString(attributes.Value());
		}
	}

	switch (type)
	{
	case HTMLInputTag::Submit:
		if (value)
		{
			parser.page.AddButton(value);
		}
		break;
	case HTMLInputTag::Text:
		parser.page.AddTextField(value, bufferLength, name);
		break;
	}
}

void FormTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	WidgetFormData* formData = parser.page.allocator.Alloc<WidgetFormData>();
	if (formData)
	{
		formData->action = NULL;
		formData->method = WidgetFormData::Get;

		AttributeParser attributes(attributeStr);
		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "action"))
			{
				formData->action = parser.page.allocator.AllocString(attributes.Value());
			}
			if (!stricmp(attributes.Key(), "method"))
			{
				if (!stricmp(attributes.Value(), "post"))
				{
					formData->method = WidgetFormData::Post;
				}
			}
		}

		parser.page.SetFormData(formData);
	}
}

void FormTagHandler::Close(HTMLParser& parser) const
{
	parser.page.SetFormData(NULL);
}
