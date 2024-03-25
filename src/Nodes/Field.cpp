#include "Field.h"
#include "../DataPack.h"
#include "../Draw/Surface.h"
#include "../Memory/Memory.h"
#include "../Layout.h"
#include "../Platform.h"
#include "../KeyCodes.h"
#include "../App.h"

void TextFieldNode::Draw(DrawContext& context, Node* node)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);

	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	uint8_t textColour = Platform::video->colourScheme.textColour;
	uint8_t buttonOutlineColour = Platform::video->colourScheme.textColour;
	uint8_t clearColour = Platform::video->colourScheme.pageColour;
	context.surface->FillRect(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, node->size.y - 2, clearColour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y, node->size.x - 2, buttonOutlineColour);
	context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 1, node->size.x - 2, buttonOutlineColour);
	context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, buttonOutlineColour);

	DrawContext subContext = context;
	subContext.clipRight = node->anchor.x + node->size.x - 2;

	if (node == App::Get().ui.GetFocusedNode())
	{
		subContext.surface->DrawString(subContext, font, data->buffer + shiftPosition, node->anchor.x + 3, node->anchor.y + 2, textColour, node->style.fontStyle);

		if (selectionLength > 0)
		{
			DrawSelection(subContext, node);
		}
		else
		{
			DrawCursor(context, node);
		}
	}
	else
	{
		subContext.surface->DrawString(subContext, font, data->buffer, node->anchor.x + 3, node->anchor.y + 2, textColour, node->style.fontStyle);
	}
}

Node* TextFieldNode::Construct(Allocator& allocator, const char* inValue, NodeCallbackFunction onSubmit)
{
	char* buffer = (char*) allocator.Allocate(DEFAULT_TEXT_FIELD_BUFFER_SIZE);
	if (!buffer)
	{
		return nullptr;
	}

	TextFieldNode::Data* data = allocator.Alloc<TextFieldNode::Data>(buffer, DEFAULT_TEXT_FIELD_BUFFER_SIZE, onSubmit);
	if (data)
	{
		if (inValue)
		{
			strncpy(buffer, inValue, DEFAULT_TEXT_FIELD_BUFFER_SIZE);
			buffer[DEFAULT_TEXT_FIELD_BUFFER_SIZE - 1] = '\0';
		}
		else
		{
			buffer[0] = '\0';
		}
		return allocator.Alloc<Node>(Node::TextField, data);
	}

	return nullptr;
}

Node* TextFieldNode::Construct(Allocator& allocator, char* buffer, int bufferLength, NodeCallbackFunction onSubmit)
{
	TextFieldNode::Data* data = allocator.Alloc<TextFieldNode::Data>(buffer, bufferLength, onSubmit);
	if (data)
	{
		return allocator.Alloc<Node>(Node::TextField, data);
	}

	return nullptr;
}

void TextFieldNode::GenerateLayout(Layout& layout, Node* node)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	if (data->explicitWidth.IsSet())
	{
		node->size.x = layout.CalculateWidth(data->explicitWidth);
	}
	else
	{
		node->size.x = Platform::video->screenWidth / 3;
	}
	node->size.y = font->glyphHeight + 4;

	if (layout.MaxAvailableWidth() < node->size.x)
	{
		node->size.x = layout.MaxAvailableWidth();
	}

	if (layout.AvailableWidth() < node->size.x)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

bool TextFieldNode::HandleEvent(Node* node, const Event& event)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);

	switch (event.type)
	{
	case Event::Focus:
		shiftPosition = 0;
		ShiftIntoView(node);

		if (App::Get().ui.IsInterfaceNode(node) && strlen(data->buffer) > 0)
		{
			// This is the address bar
			cursorPosition = strlen(data->buffer);
			selectionStartPosition = 0;
			selectionLength = cursorPosition;
			DrawSelection(context, node);
		}
		else
		{
			selectionStartPosition = selectionLength = 0;
			cursorPosition = pickedPosition != -1 ? pickedPosition : strlen(data->buffer);
			DrawCursor(context, node);
		}
		break;
	case Event::Unfocus:
		pickedPosition = -1;
		if (shiftPosition > 0)
		{
			shiftPosition = 0;
			selectionLength = 0;
			cursorPosition = -1;
			node->Redraw();
		}
		else
		{
			if (selectionLength > 0)
			{
				DrawSelection(context, node);
			}
			else
			{
				DrawCursor(context, node);
			}
		}
		break;
	case Event::MouseClick:
		pickedPosition = PickPosition(node, event.x, event.y);
		if (App::Get().ui.GetFocusedNode() != node)
		{
			App::Get().ui.FocusNode(node);
		}
		else
		{
			ClearSelection(node);
			MoveCursorPosition(node, pickedPosition);
		}
		break;
	case Event::MouseDrag:
		if(pickedPosition != -1)
		{
			int releasedPickPosition = PickPosition(node, event.x, event.y);
			if (pickedPosition != releasedPickPosition)
			{
				if (selectionLength > 0)
				{
					DrawSelection(context, node);
				}
				else
				{
					DrawCursor(context, node);
				}

				if (pickedPosition > releasedPickPosition)
				{
					selectionStartPosition = releasedPickPosition;
					selectionLength = pickedPosition - releasedPickPosition;
				}
				else
				{
					selectionStartPosition = pickedPosition;
					selectionLength = releasedPickPosition - pickedPosition;
				}

				DrawSelection(context, node);
			}
		}
		//cursorPosition = -1;
		break;
	case Event::KeyPress:
		pickedPosition = -1;
		if (event.key >= 32 && event.key < 128)
		{
			if (selectionLength > 0)
			{
				DeleteSelectionContents(node);
			}

			if (cursorPosition < data->bufferSize)
			{
				int len = strlen(data->buffer);
				for (int n = len; n >= cursorPosition; n--)
				{
					data->buffer[n + 1] = data->buffer[n];
				}
				data->buffer[cursorPosition] = (char)(event.key);
				MoveCursorPosition(node, cursorPosition + 1);
				RedrawModified(node, cursorPosition - 1);
			}
			ShiftIntoView(node);
			return true;
		}
		else if (event.key == KEYCODE_BACKSPACE)
		{
			if (selectionLength > 0)
			{
				DeleteSelectionContents(node);
			}
			else if (cursorPosition > 0)
			{
				int len = strlen(data->buffer);
				for (int n = cursorPosition - 1; n < len; n++)
				{
					data->buffer[n] = data->buffer[n + 1];
				}
				MoveCursorPosition(node, cursorPosition - 1);
				RedrawModified(node, cursorPosition);
			}
			ShiftIntoView(node);
			return true;
		}
		else if (event.key == KEYCODE_DELETE)
		{
			int len = strlen(data->buffer);
			if (selectionLength > 0)
			{
				DeleteSelectionContents(node);
			}
			else if (cursorPosition < len)
			{
				for (int n = cursorPosition; n < len; n++)
				{
					data->buffer[n] = data->buffer[n + 1];
				}
				RedrawModified(node, cursorPosition);
			}
			ShiftIntoView(node);
			return true;
		}
		else if (event.key == KEYCODE_ENTER)
		{
			if (data->onSubmit)
			{
				data->onSubmit(node);
			}
			return true;
		}
		else if (event.key == KEYCODE_ARROW_LEFT)
		{
			if (selectionLength > 0)
			{
				ClearSelection(node);
			}
			else if (cursorPosition > 0)
			{
				MoveCursorPosition(node, cursorPosition - 1);
			}
			ShiftIntoView(node);
			return true;
		}
		else if (event.key == KEYCODE_ARROW_RIGHT)
		{
			if (selectionLength > 0)
			{
				ClearSelection(node);
			}
			else if (cursorPosition < strlen(data->buffer))
			{
				MoveCursorPosition(node, cursorPosition + 1);
			}
			ShiftIntoView(node);
			return true;
		}
		else if (event.key == KEYCODE_END)
		{
			if (selectionLength > 0)
			{
				ClearSelection(node);
			}
			else
			{
				MoveCursorPosition(node, strlen(data->buffer));
			}
			ShiftIntoView(node);
			return true;
		}
		else if (event.key == KEYCODE_HOME)
		{
			if (selectionLength > 0)
			{
				ClearSelection(node);
			}
			else
			{
				MoveCursorPosition(node, 0);
			}
			ShiftIntoView(node);
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

int TextFieldNode::GetBufferPixelWidth(Node* node, int start, int end)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	int x = 0;

	for (int n = 0; n < end; n++)
	{
		if (!data->buffer[n])
			break;

		if (n >= start)
		{
			x += font->GetGlyphWidth(data->buffer[n]);
		}
	}

	return x;
}

void TextFieldNode::DrawCursor(DrawContext& context, Node* node)
{
	if (cursorPosition < 0)
		return;

	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	int x = node->anchor.x + 3;
	int height = font->glyphHeight;

	x += GetBufferPixelWidth(node, shiftPosition, cursorPosition);

	if (x >= node->anchor.x + node->size.x - 1)
	{
		return;
	}

	int y = node->anchor.y + 2;

	context.surface->InvertRect(context, x, y, 1, height);
}

void TextFieldNode::MoveCursorPosition(Node* node, int newPosition)
{
	Platform::input->HideMouse();

	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);
	DrawCursor(context, node);
	cursorPosition = newPosition;
	DrawCursor(context, node);

	Platform::input->ShowMouse();
}

void TextFieldNode::DrawSelection(DrawContext& context, Node* node)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);

	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	int selectionX1 = GetBufferPixelWidth(node, shiftPosition, selectionStartPosition);
	int selectionX2 = GetBufferPixelWidth(node, shiftPosition, selectionStartPosition + selectionLength);

	Platform::input->HideMouse();
	context.surface->InvertRect(context, selectionX1 + node->anchor.x + 3, node->anchor.y + 2, selectionX2 - selectionX1, font->glyphHeight);
	Platform::input->ShowMouse();
}

void TextFieldNode::RedrawModified(Node* node, int position)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	DrawContext context;
	uint8_t textColour = Platform::video->colourScheme.textColour;
	uint8_t clearColour = Platform::video->colourScheme.pageColour;
	context.clipRight = node->anchor.x + node->size.x - 2;

	int drawPosition = GetBufferPixelWidth(node, shiftPosition, position) + 3;
	int clearWidth = node->size.x - 1 - drawPosition;
	drawPosition += node->anchor.x;

	App::Get().pageRenderer.GenerateDrawContext(context, node);

	Platform::input->HideMouse();
	context.surface->FillRect(context, drawPosition, node->anchor.y + 1, clearWidth, node->size.y - 2, clearColour);
	context.surface->DrawString(context, font, data->buffer + position, drawPosition, node->anchor.y + 2, textColour, node->style.fontStyle);
	DrawCursor(context, node);
	Platform::input->ShowMouse();
}

void TextFieldNode::ShiftIntoView(Node* node)
{
	bool needsRedraw = false;

	if (cursorPosition < 0)
	{
		cursorPosition = 0;
	}

	if (cursorPosition < shiftPosition)
	{
		shiftPosition = cursorPosition;
		needsRedraw = true;
	}
	else
	{
		int cursorPixelPosition = GetBufferPixelWidth(node, shiftPosition, cursorPosition);

		while (cursorPixelPosition > node->size.x - 4)
		{
			needsRedraw = true;
			shiftPosition++;
			cursorPixelPosition = GetBufferPixelWidth(node, shiftPosition, cursorPosition);
		}
	}

	if (needsRedraw)
	{
		RedrawModified(node, shiftPosition);
	}
}

void TextFieldNode::DeleteSelectionContents(Node* node)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);

	strcpy(data->buffer + selectionStartPosition, data->buffer + selectionStartPosition + selectionLength);
	cursorPosition = selectionStartPosition;
	selectionLength = 0;
	RedrawModified(node, cursorPosition);
}

void TextFieldNode::ClearSelection(Node* node)
{
	if (selectionLength > 0)
	{
		DrawContext context;
		App::Get().pageRenderer.GenerateDrawContext(context, node);

		DrawSelection(context, node);
		selectionLength = 0;
		DrawCursor(context, node);
	}
}

int TextFieldNode::PickPosition(Node* node, int x, int y)
{
	x -= node->anchor.x + 3;
	int result = shiftPosition;

	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);

	while(x > 0)
	{
		if (!data->buffer[result])
		{
			break;
		}

		x -= font->GetGlyphWidth(data->buffer[result]);
		result++;
	}

	return result;
}
