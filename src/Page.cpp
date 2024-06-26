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
#include <stdio.h>
#include <string.h>
#include "Platform.h"
#include "Page.h"
#include "App.h"
#include "Image/Image.h"
#include "Nodes/Section.h"
#include "Nodes/Text.h"
#include "Nodes/Form.h"
#include "Nodes/StyNode.h"
#include "Nodes/Select.h"
#include "Nodes/LinkNode.h"
#include "Draw/Surface.h"
#include "Memory/Memory.h"

#define TOP_MARGIN_PADDING 1

Page::Page(App& inApp) : app(inApp), layout(*this)
{
}

void Page::Reset()
{
	pageWidth = app.ui.windowRect.width;
	pageHeight = 0;
	pendingVerticalPadding = 0;
	textBufferSize = 0;
	leftMarginPadding = 1;
	cursorX = leftMarginPadding;
	cursorY = TOP_MARGIN_PADDING;
	colourScheme = Platform::video->colourScheme;

	MemoryManager::pageAllocator.Reset();
	MemoryManager::pageBlockAllocator.Reset();

	rootNode = SectionElement::Construct(MemoryManager::pageAllocator, SectionElement::Document);
	ElementStyle rootStyle;
	rootStyle.alignment = ElementAlignment::Left;
	rootStyle.fontSize = 1;
	rootStyle.fontStyle = FontStyle::Regular;
	rootStyle.fontColour = colourScheme.textColour;
	rootNode->SetStyle(rootStyle);

	layout.Reset();
}

void Page::SetTitle(const char* inTitle)
{
	app.ui.SetTitle(inTitle);
}

static const char* nodeTypeNames[] =
{
	"Section",
	"Text",
	"SubText",
	"Image",
	"Break",
	"Style",
	"Link",
	"Block",
	"Button",
	"TextField",
	"Form",
	"StatusBar",
	"ScrollBar",
	"Table",
	"TableRow",
	"TableCell",
	"Select",
	"Option",
	"List",
	"ListItem"
};

void Page::DebugDumpNodeGraph()
{
	DebugDumpNodeGraph(GetRootNode());

	int nodeTypeCounts[Node::NumNodeTypes];

	memset(nodeTypeCounts, 0, sizeof(nodeTypeCounts));
	int totalCount = 0;

	for (Node* node = GetRootNode(); node; node = node->GetNextInTree())
	{
		nodeTypeCounts[node->type]++;
		totalCount++;
	}

	for (int n = 0; n < Node::NumNodeTypes; n++)
	{
		printf("%s :\t%d\n", nodeTypeNames[n], nodeTypeCounts[n]);
	}
	printf("Total: %d nodes\n", totalCount);

}

void Page::DebugDumpNodeGraph(Node* node, int depth)
{
	static const char* sectionTypeNames[] =
	{
		"Document",
		"HTML",
		"Head",
		"Body",
		"Script",
		"Style",
		"Title"
	};

	for (int i = 0; i < depth; i++)
	{
		printf(" ");
	}
	
	switch (node->type)
	{
		case Node::Text:
		{
			TextElement::Data* data = static_cast<TextElement::Data*>(node->data);
			if (node->firstChild)
			{
				printf("<%s> [%d,%d:%d,%d]\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y);
			}
			else
			{
				printf("<%s> [%d,%d:%d,%d] %s\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y, data->text.Get<char*>());
			}
		}
		break;
	case Node::SubText:
		{
			TextElement::Data* data = static_cast<TextElement::Data*>(node->parent->data);
			SubTextElement::Data* subData = static_cast<SubTextElement::Data*>(node->data);
			char* text = data->text.Get<char*>() + subData->startIndex;
			char temp = text[subData->length];
			text[subData->length] = 0;
			printf("<%s> [%d,%d:%d,%d] %s\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y, text);
			text[subData->length] = temp;
		}
		break;
	case Node::Option:
		{
			OptionNode::Data* data = static_cast<OptionNode::Data*>(node->data);
			printf("<%s> [%s]\n", nodeTypeNames[node->type], data->text);
		}
		break;
	case Node::Link:
		{
			LinkNode::Data* data = static_cast<LinkNode::Data*>(node->data);
			printf("<%s> [%d,%d:%d,%d] %s\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y, data->url);
		}
		break;
	case Node::Section:
		{
			SectionElement::Data* data = static_cast<SectionElement::Data*>(node->data);
			printf("<%s> [%d,%d:%d,%d] %s\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y, sectionTypeNames[data->type]);
		}
		break;
	case Node::Form:
		{
			FormNode::Data* data = static_cast<FormNode::Data*>(node->data);
			printf("<%s> [%d,%d:%d,%d] action: %s\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y, data->action ? data->action : "NONE");
		}
		break;
	case Node::Style:
		{
			StyleNode::Data* data = static_cast<StyleNode::Data*>(node->data);
			printf("<%s> [%d,%d:%d,%d] ", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y);
			if (data->styleOverride.overrideMask.alignment)
			{
				switch (data->styleOverride.styleSettings.alignment)
				{
				case ElementAlignment::Center:
					printf("align center ");
					break;
				}
			}
			if (data->styleOverride.overrideMask.fontStyle)
			{
				if (data->styleOverride.styleSettings.fontStyle & FontStyle::Bold)
				{
					printf("bold ");
				}
				if (data->styleOverride.styleSettings.fontStyle & FontStyle::Italic)
				{
					printf("italic ");
				}
				if (data->styleOverride.styleSettings.fontStyle & FontStyle::Underline)
				{
					printf("underline ");
				}
			}

			printf("\n");
		}
		break;
	default:
		printf("<%s> [%d,%d:%d,%d]\n", nodeTypeNames[node->type], node->anchor.x, node->anchor.y, node->size.x, node->size.y);
		break;
	}

	for (Node* child = node->firstChild; child; child = child->next)
	{
		DebugDumpNodeGraph(child, depth + 1);
	}
}

Node* Page::ProcessNextLoadTask(Node* lastNode, LoadTask& loadTask)
{
	Node* node;
	for (node = lastNode->GetNextInTree(); node; node = node->GetNextInTree())
	{
		if (node && node->type == Node::Image)
		{
			node->Handler().LoadContent(node, loadTask);
			return node;
		}
	}

	return nullptr;
}
