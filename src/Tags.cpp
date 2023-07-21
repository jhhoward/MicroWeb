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
#include "DataPack.h"

#include "Nodes/Section.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Break.h"
#include "Nodes/StyNode.h"
#include "Nodes/LinkNode.h"
#include "Nodes/Block.h"
#include "Nodes/Button.h"
#include "Nodes/Field.h"
#include "Nodes/Form.h"

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

void HrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	// TODO-refactor
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void BrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void HTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
	parser.PushContext(StyleNode::ConstructFontStyle(parser.page.allocator, FontStyle::Bold, size >= 3 ? 1 : 2), this);
}

void HTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void SizeTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushContext(StyleNode::ConstructFontSize(parser.page.allocator, size), this);
}

void SizeTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void LiTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	// TODO-refactor : add bullet point
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}
void LiTagHandler::Close(class HTMLParser& parser) const
{
}

void ATagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	char* url = NULL;

	while(attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "href"))
		{
			url = parser.page.allocator.AllocString(attributes.Value());
		}
	}

	parser.PushContext(LinkNode::Construct(parser.page.allocator, url), this);
}

void ATagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void BlockTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	// TODO-refactor
	int lineHeight = Assets.GetFont(1, FontStyle::Regular)->glyphHeight;
	parser.PushContext(BlockNode::Construct(parser.page.allocator, leftMarginPadding, useVerticalPadding ? lineHeight >> 1 : 0), this);
}
void BlockTagHandler::Close(class HTMLParser& parser) const
{
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
	parser.PushContext(StyleNode::ConstructAlignment(parser.page.allocator, alignmentType), this);
}

void AlignmentTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void FontTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	// TODO-refactor
}

void FontTagHandler::Close(class HTMLParser& parser) const
{
}

void ListTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	// TODO-refactor
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}

void ListTagHandler::Close(class HTMLParser& parser) const
{
	// TODO-refactor
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

	parser.EmitNode(ButtonNode::Construct(parser.page.allocator, title, NULL));
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
			parser.EmitNode(ButtonNode::Construct(parser.page.allocator, value, FormNode::OnSubmitButtonPressed));
		}
		break;
	case HTMLInputTag::Text:
		{
			Node* fieldNode = TextFieldNode::Construct(parser.page.allocator, value, FormNode::OnSubmitButtonPressed);
			if (fieldNode && fieldNode->data)
			{
				TextFieldNode::Data* fieldData = static_cast<TextFieldNode::Data*>(fieldNode->data);
				fieldData->name = name;
				parser.EmitNode(fieldNode);
			}
		}
		break;
	}
}

void FormTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* formNode = FormNode::Construct(parser.page.allocator);
	parser.PushContext(formNode, this);

	FormNode::Data* formData = static_cast<FormNode::Data*>(formNode->data);

	if (formData)
	{
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
					formData->method = FormNode::Data::Post;
				}
			}
		}
	}
}

void FormTagHandler::Close(HTMLParser& parser) const
{
	parser.PopContext(this);
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
		Image* imageIcon = Assets.imageIcon;
		if (imageIcon)
		{
			parser.EmitImage(imageIcon, imageIcon->width, imageIcon->height);
		}

		if (altText)
		{
			parser.EmitText(altText);
		}
	}
	else
	{
		Platform::video->ScaleImageDimensions(width, height);
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
	// TODO-refactor
	parser.PushPreFormatted();
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
	parser.PushContext(StyleNode::ConstructFontStyle(parser.page.allocator, FontStyle::Monospace), this);
}

void PreformattedTagHandler::Close(class HTMLParser& parser) const
{
	// TODO-refactor
	parser.PopPreFormatted();
	parser.PopContext(this);
	parser.EmitNode(BreakNode::Construct(parser.page.allocator));
}
