
#include "Button.h"
#include "../DataPack.h"
#include "../Draw/Surface.h"
#include "../Memory/LinAlloc.h"
#include "../Layout.h"
#include "../Event.h"
#include "../App.h"
#include "../KeyCodes.h"

void ButtonNode::Draw(DrawContext& context, Node* node)
{
	ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);

	if (data->buttonText)
	{
		Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
		uint8_t textColour = Platform::video->colourScheme.textColour;
		uint8_t buttonOutlineColour = Platform::video->colourScheme.textColour;
		uint8_t buttonColour = Platform::video->colourScheme.buttonColour;
		context.surface->FillRect(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, node->size.y - 2, buttonColour);
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y, node->size.x - 2, buttonOutlineColour);
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 1, node->size.x - 2, buttonOutlineColour);
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 2, node->size.x - 2, buttonOutlineColour);
		context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
		context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
		context.surface->DrawString(context, font, data->buttonText, node->anchor.x + 8, node->anchor.y + 2, textColour, node->style.fontStyle);

		if (App::Get().ui.GetFocusedNode() == node)
		{
			context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, buttonOutlineColour);
			context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 3, node->size.x - 2, buttonOutlineColour);
			context.surface->VLine(context, node->anchor.x + 1, node->anchor.y + 2, node->size.y - 5, buttonOutlineColour);
			context.surface->VLine(context, node->anchor.x + node->size.x - 2, node->anchor.y + 2, node->size.y - 5, buttonOutlineColour);
		}
	}
}

Node* ButtonNode::Construct(Allocator& allocator, const char* inButtonText, NodeCallbackFunction callback)
{
	char* buttonText = NULL;
	if (inButtonText)
	{
		buttonText = allocator.AllocString(inButtonText);
		if (!buttonText)
		{
			return nullptr;
		}
		HTMLParser::ReplaceAmpersandEscapeSequences(buttonText);
	}

	ButtonNode::Data* data = allocator.Alloc<ButtonNode::Data>(buttonText, callback);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Button, data);
	}

	return nullptr;
}

Coord ButtonNode::CalculateSize(Node* node)
{
	ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	int labelHeight = font->glyphHeight;
	int labelWidth = 0;

	if (data->buttonText)
	{
		labelWidth = font->CalculateWidth(data->buttonText);
	}

	Coord result;
	result.x = labelWidth + 16;
	result.y = labelHeight + 4;
	return result;
}

void ButtonNode::GenerateLayout(Layout& layout, Node* node)
{
	//ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);

	node->size = CalculateSize(node);

	if (layout.AvailableWidth() < node->size.x)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

bool ButtonNode::HandleEvent(Node* node, const Event& event)
{
	AppInterface& ui = App::Get().ui;
	ButtonNode::Data* data = static_cast<ButtonNode::Data*>(node->data);

	switch (event.type)
	{
	case Event::MouseClick:
		{
			focusingFromMouseClick = true;
			ui.FocusNode(node);
			InvertButton(node);
		}
		return true;
	case Event::MouseRelease:
		{
			InvertButton(node);
			ui.FocusNode(nullptr);
			if (ui.IsOverNode(node, event.x, event.y))
			{
				if (data->onClick)
				{
					data->onClick(node);
				}
			}
		}
		return true;
	case Event::KeyPress:
		if (event.key == KEYCODE_ENTER)
		{
			if (data->onClick)
			{
				data->onClick(node);
			}
			return true;
		}
		return false;
	case Event::Focus:
		if (!focusingFromMouseClick)
		{
			HighlightButton(node, Platform::video->colourScheme.textColour);
		}
		else focusingFromMouseClick = false;
		return true;
	case Event::Unfocus:
		HighlightButton(node, Platform::video->colourScheme.buttonColour);
		return true;
	default:
		break;
	}

	return false;
}

void ButtonNode::InvertButton(Node* node)
{
	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);

	Platform::input->HideMouse();
	context.surface->InvertRect(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, node->size.y - 3);
	Platform::input->ShowMouse();
}

void ButtonNode::HighlightButton(Node* node, uint8_t colour)
{
	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);

	Platform::input->HideMouse();
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, colour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 3, node->size.x - 2, colour);
	context.surface->VLine(context, node->anchor.x + 1, node->anchor.y + 2, node->size.y - 5, colour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 2, node->anchor.y + 2, node->size.y - 5, colour);
	Platform::input->ShowMouse();
}
