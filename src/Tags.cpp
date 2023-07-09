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
#include "Image.h"

#include "Nodes/Section.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Break.h"
#include "Nodes/StyNode.h"
#include "Nodes/LinkNode.h"
#include "Nodes/Block.h"
#include "Nodes/Button.h"
#include "Nodes/Field.h"

static const HTMLTagHandler* tagHandlers[] =
{
	new HTMLTagHandler("generic"),
	new SectionTagHandler("html", SectionElement::HTML),
	new SectionTagHandler("head", SectionElement::Head),
	new SectionTagHandler("body", SectionElement::Body),
	new SectionTagHandler("script", SectionElement::Script),
	new SectionTagHandler("style", SectionElement::Style),
	new SectionTagHandler("title", SectionElement::Title),
	new HTagHandler("h1", 1),
	new HTagHandler("h2", 2),
	new HTagHandler("h3", 3),
	new HTagHandler("h4", 4),
	new HTagHandler("h5", 5),
	new HTagHandler("h6", 6),
	new BlockTagHandler("blockquote", true, 16),
	new BlockTagHandler("section"),
	new BlockTagHandler("p"),
	new BlockTagHandler("div", false),
	new BlockTagHandler("dt", false),
	new BlockTagHandler("dd", false, 16),
	new BlockTagHandler("tr", false),			// Table rows shouldn't really be a block but we don't have table support yet
	new BlockTagHandler("ul", true, 16),
	new BrTagHandler(),
	new AlignmentTagHandler("center", ElementAlignment::Center),
	new FontTagHandler(),
	new StyleTagHandler("b", FontStyle::Bold),
	new StyleTagHandler("strong", FontStyle::Bold),
	new StyleTagHandler("i", FontStyle::Italic),
	new StyleTagHandler("em", FontStyle::Italic),
	new StyleTagHandler("cite", FontStyle::Italic),
	new StyleTagHandler("var", FontStyle::Italic),
	new StyleTagHandler("u", FontStyle::Underline),
	new StyleTagHandler("code", FontStyle::Monospace),
	new ATagHandler(),
	new ListTagHandler("ul"),
	new ListTagHandler("ol"),
	new ListTagHandler("menu"),
	new LiTagHandler(),
	new HrTagHandler(),
	new SizeTagHandler("small", 0),
	new InputTagHandler(),
	new ButtonTagHandler(),
	new FormTagHandler(),
	new ImgTagHandler(),
	new MetaTagHandler(),
	new PreformattedTagHandler("pre"),
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

void HTMLTagHandler::ApplyStyleAttributes(WidgetStyle& style, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);

	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "align"))
		{
			if (!stricmp(attributes.Value(), "center"))
			{
				style.center = true;
			}
			else if (!stricmp(attributes.Value(), "left"))
			{
				style.center = false;
			}
		}
	}
}

void HrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//parser.page.AddHorizontalRule();
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void BrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//parser.page.BreakTextLine();
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void HTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//currentStyle.fontSize = size >= 3 ? 1 : 2;
	//currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | FontStyle::Bold);
	//ApplyStyleAttributes(currentStyle, attributeStr);
	//parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
	//parser.page.PushStyle(currentStyle);

	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
	parser.PushContext(StyleNode::ConstructFontStyle(parser.page.allocator, FontStyle::Bold, size >= 3 ? 1 : 2), this);
}

void HTagHandler::Close(class HTMLParser& parser) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
	//parser.page.PopStyle();

	parser.PopContext(this);
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void SizeTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushContext(StyleNode::ConstructFontSize(parser.page.allocator, size), this);

	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//currentStyle.fontSize = size;
	//parser.page.PushStyle(currentStyle);
}

void SizeTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);

	//parser.page.PopStyle();
}

void LiTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//int bulletPointWidth = Platform::video->GetFont(currentStyle.fontSize, currentStyle.fontStyle)->CalculateWidth(" * ", currentStyle.fontStyle);

	parser.EmitNode(BreakNode::Construct(parser.page.allocator));

	//parser.page.BreakLine();
	//parser.page.AddBulletPoint();
	//parser.page.AdjustLeftMargin(bulletPointWidth);
}
void LiTagHandler::Close(class HTMLParser& parser) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//int bulletPointWidth = Platform::video->GetFont(currentStyle.fontSize, currentStyle.fontStyle)->CalculateWidth(" * ", currentStyle.fontStyle);
	//
	//parser.page.AdjustLeftMargin(-bulletPointWidth);
	//parser.page.BreakLine();
}

void ATagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	while(attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "href"))
		{
			//parser.page.SetWidgetURL(attributes.Value());
		}
	}

	parser.PushContext(LinkNode::Construct(parser.page.allocator), this);

	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | FontStyle::Underline);
	//parser.page.PushStyle(currentStyle);

}
void ATagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
	//parser.page.ClearWidgetURL();
	//parser.page.PopStyle();
}

void BlockTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.AdjustLeftMargin(leftMarginPadding);
	//parser.page.BreakLine(useVerticalPadding ? Platform::video->GetLineHeight(currentStyle.fontSize) >> 1 : 0);
	//ApplyStyleAttributes(currentStyle, attributeStr);
	//parser.page.PushStyle(currentStyle);

	parser.PushContext(BlockNode::Construct(parser.page.allocator, leftMarginPadding, useVerticalPadding ? Platform::video->GetLineHeight(1) >> 1 : 0), this);
}
void BlockTagHandler::Close(class HTMLParser& parser) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.AdjustLeftMargin(-leftMarginPadding);
	//parser.page.BreakLine(useVerticalPadding ? Platform::video->GetLineHeight(currentStyle.fontSize) >> 1 : 0);
	//parser.page.PopStyle();

	parser.PopContext(this);
}

void SectionTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushContext(SectionElement::Construct(parser.page.allocator, sectionType), this);
}
void SectionTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void StyleTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | style);
	//parser.page.PushStyle(currentStyle);
	Node* styleNode = StyleNode::ConstructFontStyle(parser.page.allocator, style);
	if (styleNode)
	{
		parser.PushContext(styleNode, this);
	}
}

void StyleTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void AlignmentTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//
	//parser.page.BreakLine();
	//
	//currentStyle.center = true;
	//parser.page.PushStyle(currentStyle);
	parser.PushContext(StyleNode::ConstructAlignment(parser.page.allocator, alignmentType), this);
}

void AlignmentTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
	//parser.page.PopStyle();
	//parser.page.BreakLine();
}

void FontTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	/*
	WidgetStyle currentStyle = parser.page.GetStyleStackTop();

	AttributeParser attributes(attributeStr);
	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "size"))
		{
			int size = atoi(attributes.Value());

			if (size < 0)
			{
				// Relative sizing
				if (currentStyle.fontSize + size < 0)
				{
					currentStyle.fontSize = 0;
				}
				else
				{
					currentStyle.fontSize += size;
				}
			}
			else
			{
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
	}

	parser.page.PushStyle(currentStyle);
	*/
}

void FontTagHandler::Close(class HTMLParser& parser) const
{
	//parser.page.PopStyle();
}

void ListTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void ListTagHandler::Close(class HTMLParser& parser) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize) >> 1);
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
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

void ButtonTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	char* title = NULL;

	AttributeParser attributes(attributeStr);
	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "title"))
		{
			title = parser.page.allocator.AllocString(attributes.Value());
		}
	}

	parser.EmitNode(ButtonNode::Construct(parser.page.allocator, title));
}

void InputTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	char* value = NULL;
	char* name = NULL;
	HTMLInputTag::Type type = HTMLInputTag::Text;
	int bufferLength = 80;

	AttributeParser attributes(attributeStr);
	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "type"))
		{
			if (!stricmp(attributes.Value(), "submit") || !stricmp(attributes.Value(), "button"))
			{
				type = HTMLInputTag::Submit;
			}
			else if (!stricmp(attributes.Value(), "text") || !stricmp(attributes.Value(), "search"))
			{
				type = HTMLInputTag::Text;
			}
			else
			{
				type = HTMLInputTag::Unknown;
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
			parser.EmitNode(ButtonNode::Construct(parser.page.allocator, value));
		}
		break;
	case HTMLInputTag::Text:
		parser.EmitNode(TextFieldNode::Construct(parser.page.allocator, value));
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

void ImgTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	int width = -1;
	int height = -1;
	char* altText = NULL;

	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "alt"))
		{
			altText = (char*) attributes.Value();
		}
		if (!stricmp(attributes.Key(), "width"))
		{
			width = atoi(attributes.Value());
		}
		if (!stricmp(attributes.Key(), "height"))
		{
			height = atoi(attributes.Value());
		}
	}

	if (width == -1)
	{
		if (height != -1)
		{
			width = height;
		}
	}
	if (height == -1)
	{
		if (width != -1)
		{
			height = width;
		}
	}

	if (width == -1 && height == -1)
	{
		Image* imageIcon = Platform::video->imageIcon;
		if (imageIcon)
		{
			//parser.page.AddImage(NULL, imageIcon->width, imageIcon->height);
			parser.EmitImage(NULL, imageIcon->width, imageIcon->height);
		}

		if (altText)
		{
			parser.page.AppendText(altText);
		}
	}
	else
	{
		Platform::video->ScaleImageDimensions(width, height);
		//parser.page.AddImage(altText, width, height);
		parser.EmitImage(NULL, width, height);
	}
}

void MetaTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);

	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "charset"))
		{
			if (!stricmp(attributes.Value(), "utf-8"))
			{
				parser.SetTextEncoding(TextEncoding::UTF8);
			}
			else if (!stricmp(attributes.Value(), "ISO-8859-1") || !stricmp(attributes.Value(), "windows-1252"))
			{
				parser.SetTextEncoding(TextEncoding::ISO_8859_1);
			}
			else if (!stricmp(attributes.Value(), "ISO-8859-2") || !stricmp(attributes.Value(), "windows-1250"))
			{
				parser.SetTextEncoding(TextEncoding::ISO_8859_2);
			}
		}
		else if (!stricmp(attributes.Key(), "content"))
		{
			if (strstr(attributes.Value(), "charset=utf-8"))
			{
				parser.SetTextEncoding(TextEncoding::UTF8);
			}
			else if (strstr(attributes.Value(), "charset=ISO-8859-1") || strstr(attributes.Value(), "charset=windows-1252"))
			{
				parser.SetTextEncoding(TextEncoding::ISO_8859_1);
			}
			else if (strstr(attributes.Value(), "charset=ISO-8859-2") || strstr(attributes.Value(), "charset=windows-1250"))
			{
				parser.SetTextEncoding(TextEncoding::ISO_8859_2);
			}
		}
	}
}

void PreformattedTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize, currentStyle.fontStyle) >> 1);
	//
	//currentStyle.fontStyle = (FontStyle::Type)(currentStyle.fontStyle | FontStyle::Monospace);
	//parser.page.PushStyle(currentStyle);
	//parser.PushPreFormatted();
	//parser.page.BreakTextLine();
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void PreformattedTagHandler::Close(class HTMLParser& parser) const
{
	//parser.page.PopStyle();
	//parser.PopPreFormatted();
	//
	//WidgetStyle currentStyle = parser.page.GetStyleStackTop();
	//parser.page.BreakLine(Platform::video->GetLineHeight(currentStyle.fontSize, currentStyle.fontStyle) >> 1);
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}
