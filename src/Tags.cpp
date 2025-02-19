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
#include "App.h"
#include "Image/Image.h"
#include "DataPack.h"
#include "Memory/Memory.h"

#include "Nodes/Section.h"
#include "Nodes/ImgNode.h"
#include "Nodes/Break.h"
#include "Nodes/StyNode.h"
#include "Nodes/LinkNode.h"
#include "Nodes/Block.h"
#include "Nodes/Button.h"
#include "Nodes/Field.h"
#include "Nodes/Form.h"
#include "Nodes/Table.h"
#include "Nodes/Select.h"
#include "Nodes/ListItem.h"
#include "Nodes/Text.h"
#include "Nodes/CheckBox.h"

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
	new InputTagHandler("input"),
	new InputTagHandler("textarea"),
	new ButtonTagHandler(),
	new FormTagHandler(),
	new ImgTagHandler(),
	new MetaTagHandler(),
	new PreformattedTagHandler("pre"),
	new TableTagHandler(),
	new TableRowTagHandler(),
	new TableCellTagHandler("td", false),
	new TableCellTagHandler("th", true),
	new SelectTagHandler(),
	new OptionTagHandler(),
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
	int padding = Assets.GetFont(1, FontStyle::Bold)->glyphHeight;
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator, padding, true));
}

void BrTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	int fontSize = parser.CurrentContext().node->GetStyle().fontSize;
	int padding = Assets.GetFont(fontSize, FontStyle::Regular)->glyphHeight;
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator, padding, false, true));
}

void HTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	int fontSize = size >= 3 ? 1 : 2;
	int padding = Assets.GetFont(fontSize, FontStyle::Bold)->glyphHeight / 2;
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator, padding));
	parser.PushContext(StyleNode::ConstructFontStyle(MemoryManager::pageAllocator, FontStyle::Bold, fontSize), this);
}

void HTagHandler::Close(class HTMLParser& parser) const
{
	int fontSize = size >= 3 ? 1 : 2;
	int padding = Assets.GetFont(fontSize, FontStyle::Bold)->glyphHeight / 2;
	parser.PopContext(this);
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator, padding));
}

void SizeTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushContext(StyleNode::ConstructFontSize(MemoryManager::pageAllocator, size), this);
}

void SizeTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void LiTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	if (parser.CurrentContext().tag == this)
	{
		// Have to do this because sometimes people dont close their <li> tag and just
		// treat them as bullet point markers
		parser.PopContext(this);
	}
	parser.PushContext(ListItemNode::Construct(MemoryManager::pageAllocator), this);
}
void LiTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void ATagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	char* url = NULL;

	while(attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "href"))
		{
			url = MemoryManager::pageAllocator.AllocString(attributes.Value());
		}
	}

	parser.PushContext(LinkNode::Construct(MemoryManager::pageAllocator, url), this);
}

void ATagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void BlockTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	// TODO-refactor
	int lineHeight = Assets.GetFont(1, FontStyle::Regular)->glyphHeight;
	parser.PushContext(BlockNode::Construct(MemoryManager::pageAllocator, leftMarginPadding, useVerticalPadding ? lineHeight >> 1 : 0), this);
}
void BlockTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void SectionTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* node = SectionElement::Construct(MemoryManager::pageAllocator, sectionType);
	parser.PushContext(node, this);

	if (sectionType == SectionElement::Body)
	{
		AttributeParser attributes(attributeStr);
		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "link"))
			{
				parser.page.colourScheme.linkColour = HTMLParser::ParseColourCode(attributes.Value());
			}
			if (!stricmp(attributes.Key(), "text"))
			{
				parser.page.colourScheme.textColour = HTMLParser::ParseColourCode(attributes.Value());
			}
			if (!stricmp(attributes.Key(), "bgcolor"))
			{
				parser.page.colourScheme.pageColour = HTMLParser::ParseColourCode(attributes.Value());
				App::Get().pageRenderer.RefreshAll();
			}
		}
	}

	if (node)
	{
		ElementStyle style = node->GetStyle();
		style.fontColour = parser.page.colourScheme.textColour;
		node->SetStyle(style);
	}
}
void SectionTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void StyleTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* styleNode = StyleNode::ConstructFontStyle(MemoryManager::pageAllocator, style);
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
	parser.PushContext(StyleNode::ConstructAlignment(MemoryManager::pageAllocator, alignmentType), this);
}

void AlignmentTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator));
}

void FontTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* styleNode = StyleNode::Construct(MemoryManager::pageAllocator);
	if (styleNode)
	{
		StyleNode::Data* data = static_cast<StyleNode::Data*>(styleNode->data);

		AttributeParser attributes(attributeStr);
		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "size"))
			{
				int fontSize = atoi(attributes.Value());

				if (attributes.Value()[0] == '+' || attributes.Value()[0] == '-')
				{
					data->styleOverride.SetFontSizeDelta(fontSize);
				}
				else
				{
					switch (fontSize)
					{
					case 1:
					case 2:
						data->styleOverride.SetFontSize(0);
						break;
					case 0:
						// Probably invalid
					case 3:
					case 4:
						data->styleOverride.SetFontSize(1);
						break;
					default:
						// Anything bigger
						data->styleOverride.SetFontSize(2);
						break;
					}
				}
			}
			else if (!stricmp(attributes.Key(), "color"))
			{
				if (Platform::video->paletteLUT)
				{
					data->styleOverride.SetFontColour(HTMLParser::ParseColourCode(attributes.Value()));
				}
			}
		}

		parser.PushContext(styleNode, this);
	}
}

void FontTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void ListTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushContext(ListNode::Construct(MemoryManager::pageAllocator), this);
}

void ListTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

struct HTMLInputTag
{
	enum Type
	{
		Unknown,
		Submit,
		Text,
		CheckBox,
		Radio,
		Password
	};
};

void ButtonTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	AttributeParser attributes(attributeStr);
	while (attributes.Parse())
	{
	}

	parser.PushContext(ButtonNode::Construct(MemoryManager::pageAllocator, NULL, FormNode::OnSubmitButtonPressed), this);
}

void ButtonTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void InputTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	char* value = NULL;
	char* name = NULL;
	bool checked = false;
	HTMLInputTag::Type type = HTMLInputTag::Text;
	int bufferLength = 80;
	ExplicitDimension width;

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
			else if (!stricmp(attributes.Value(), "password"))
			{
				type = HTMLInputTag::Password;
			}
			else if (!stricmp(attributes.Value(), "checkbox"))
			{
				type = HTMLInputTag::CheckBox;
			}
			else if (!stricmp(attributes.Value(), "radio"))
			{
				type = HTMLInputTag::Radio;
			}
			else
			{
				type = HTMLInputTag::Unknown;
			}
		}
		if (!stricmp(attributes.Key(), "value"))
		{
			value = MemoryManager::pageAllocator.AllocString(attributes.Value());
		}
		if (!stricmp(attributes.Key(), "name"))
		{
			name = MemoryManager::pageAllocator.AllocString(attributes.Value());
		}
		if (!stricmp(attributes.Key(), "width"))
		{
			width = ExplicitDimension::Parse(attributes.Value());
		}
		if (!stricmp(attributes.Key(), "checked"))
		{
			checked = true;
		}
	}

	switch (type)
	{
	case HTMLInputTag::Submit:
		if (value)
		{
			parser.EmitNode(ButtonNode::Construct(MemoryManager::pageAllocator, value, FormNode::OnSubmitButtonPressed));
		}
		break;
	case HTMLInputTag::Text:
	case HTMLInputTag::Password:
		{
			Node* fieldNode = TextFieldNode::Construct(MemoryManager::pageAllocator, value, FormNode::OnSubmitButtonPressed);
			if (fieldNode && fieldNode->data)
			{
				TextFieldNode::Data* fieldData = static_cast<TextFieldNode::Data*>(fieldNode->data);
				fieldData->name = name;
				fieldData->explicitWidth = width;
				fieldData->isPassword = type == HTMLInputTag::Password;
				parser.EmitNode(fieldNode);
			}
		}
		break;
	case HTMLInputTag::CheckBox:
	case HTMLInputTag::Radio:
	{
		Node* fieldNode = CheckBoxNode::Construct(MemoryManager::pageAllocator, name, value, type == HTMLInputTag::Radio, checked);
		if (fieldNode)
		{
			parser.EmitNode(fieldNode);
		}
	}
	break;
	}
}

void FormTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator));

	Node* formNode = FormNode::Construct(MemoryManager::pageAllocator);
	parser.PushContext(formNode, this);

	FormNode::Data* formData = static_cast<FormNode::Data*>(formNode->data);

	if (formData)
	{
		AttributeParser attributes(attributeStr);
		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "action"))
			{
				formData->action = MemoryManager::pageAllocator.AllocString(attributes.Value());
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
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator));
}

void ImgTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* imageNode = ImageNode::Construct(MemoryManager::pageAllocator);
	if (imageNode)
	{
		ImageNode::Data* data = static_cast<ImageNode::Data*>(imageNode->data);
		if (data)
		{
			AttributeParser attributes(attributeStr);

			while (attributes.Parse())
			{
				if (!stricmp(attributes.Key(), "alt"))
				{
					data->altText = MemoryManager::pageAllocator.AllocString(attributes.Value());
					if (data->altText)
					{
						HTMLParser::ReplaceAmpersandEscapeSequences(data->altText);
					}
				}
				else if (!stricmp(attributes.Key(), "src"))
				{
					data->source = MemoryManager::pageAllocator.AllocString(attributes.Value());
				}
				else if (!stricmp(attributes.Key(), "width"))
				{
					data->explicitWidth = ExplicitDimension::Parse(attributes.Value());
				}
				else if (!stricmp(attributes.Key(), "height"))
				{
					data->explicitHeight = ExplicitDimension::Parse(attributes.Value());
				}
				else if (!stricmp(attributes.Key(), "ismap"))
				{
					data->isMap = true;
				}
			}

			parser.EmitNode(imageNode);
		}
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
	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator));

	int lineHeight = Assets.GetFont(1, FontStyle::Regular)->glyphHeight;
	int padding = lineHeight >> 1;
	parser.PushContext(BlockNode::Construct(MemoryManager::pageAllocator, padding, padding), this);
	parser.PushContext(StyleNode::ConstructFontStyle(MemoryManager::pageAllocator, FontStyle::Monospace), this);
}

void PreformattedTagHandler::Close(class HTMLParser& parser) const
{
	// TODO-refactor
	parser.PopPreFormatted();
	parser.PopContext(this);
	parser.PopContext(this);
//	parser.EmitNode(BreakNode::Construct(MemoryManager::pageAllocator));
}

void TableTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* tableNode = TableNode::Construct(MemoryManager::pageAllocator);
	if (tableNode)
	{
		TableNode::Data* tableNodeData = static_cast<TableNode::Data*>(tableNode->data);

		AttributeParser attributes(attributeStr);

		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "border"))
			{
				tableNodeData->border = attributes.ValueAsInt();
			}
			else if (!stricmp(attributes.Key(), "cellpadding"))
			{
				tableNodeData->cellPadding = attributes.ValueAsInt();
			}
			else if (!stricmp(attributes.Key(), "cellSpacing"))
			{
				tableNodeData->cellSpacing = attributes.ValueAsInt();
			}
			else if (!stricmp(attributes.Key(), "bgcolor"))
			{
				tableNodeData->bgColour = HTMLParser::ParseColourCode(attributes.Value());
			}
			else if (!stricmp(attributes.Key(), "width"))
			{
				tableNodeData->explicitWidth = ExplicitDimension::Parse(attributes.Value());
			}
		}

		parser.PushContext(tableNode, this);
	}
}

void TableTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void TableRowTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	parser.PushContext(TableRowNode::Construct(MemoryManager::pageAllocator), this);
}

void TableRowTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void TableCellTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* cellNode = TableCellNode::Construct(MemoryManager::pageAllocator, isHeader);
	if (cellNode)
	{
		TableCellNode::Data* cellData = static_cast<TableCellNode::Data*>(cellNode->data);

		AttributeParser attributes(attributeStr);

		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "bgcolor"))
			{
				cellData->bgColour = HTMLParser::ParseColourCode(attributes.Value());
			}
			else if (!stricmp(attributes.Key(), "colspan"))
			{
				cellData->columnSpan = attributes.ValueAsInt();
				if (cellData->columnSpan <= 0)
				{
					cellData->columnSpan = 1;
				}
			}
			else if (!stricmp(attributes.Key(), "width"))
			{
				cellData->explicitWidth = ExplicitDimension::Parse(attributes.Value());
			}
		}
		parser.PushContext(cellNode, this);
	}
}

void TableCellTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void SelectTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	const char* name = nullptr;
	AttributeParser attributes(attributeStr);

	while (attributes.Parse())
	{
		if (!stricmp(attributes.Key(), "name"))
		{
			name = attributes.Value();
		}
	}

	parser.PushContext(SelectNode::Construct(MemoryManager::pageAllocator, name), this);
}

void SelectTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

void OptionTagHandler::Open(class HTMLParser& parser, char* attributeStr) const
{
	Node* optionNode = OptionNode::Construct(MemoryManager::pageAllocator);

	if (optionNode)
	{
		parser.PushContext(optionNode, this);

		AttributeParser attributes(attributeStr);

		while (attributes.Parse())
		{
			if (!stricmp(attributes.Key(), "selected"))
			{
				SelectNode::Data* selectData = optionNode->FindParentDataOfType<SelectNode::Data>(Node::Select);
				OptionNode::Data* optionData = static_cast<OptionNode::Data*>(optionNode->data);
				if (selectData && optionData)
				{
					selectData->selected = optionData;
				}
			}
		}
	}
}

void OptionTagHandler::Close(class HTMLParser& parser) const
{
	parser.PopContext(this);
}

