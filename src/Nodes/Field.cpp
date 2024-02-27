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
	context.surface->DrawString(context, font, data->buffer, node->anchor.x + 3, node->anchor.y + 2, textColour, node->style.fontStyle);

	if (node == App::Get().ui.GetFocusedNode())
	{
		DrawCursor(context, node, false);
	}
}

Node* TextFieldNode::Construct(Allocator& allocator, const char* inValue, NodeCallbackFunction onSubmit)
{
	char* buffer = (char*) allocator.Alloc(DEFAULT_TEXT_FIELD_BUFFER_SIZE);
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

	node->size.x = Platform::video->screenWidth / 3;
	node->size.y = font->glyphHeight + 4;

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

	switch (event.type)
	{
	case Event::MouseRelease:
		cursorPosition = -1;
		break;
	case Event::KeyPress:
		if (event.key >= 32 && event.key < 128)
		{
			if (cursorPosition == -1)
			{
				data->buffer[0] = '\0';
				cursorPosition = 0;
				node->Redraw();
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
			return true;
		}
		else if (event.key == KEYCODE_BACKSPACE)
		{
			if (cursorPosition == -1)
			{
				data->buffer[0] = '\0';
				node->Redraw();
				MoveCursorPosition(node, 0);
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
			return true;
		}
		else if (event.key == KEYCODE_DELETE)
		{
			int len = strlen(data->buffer);
			if (cursorPosition == -1)
			{
				data->buffer[0] = '\0';
				node->Redraw();
			}
			else if (cursorPosition < len)
			{
				for (int n = cursorPosition; n < len; n++)
				{
					data->buffer[n] = data->buffer[n + 1];
				}
				RedrawModified(node, cursorPosition);
			}
			return true;
		}
		else if (event.key == KEYCODE_ENTER)
		{
			if (data->onSubmit)
			{
				data->onSubmit(node);
			}
			/*if (activeWidget == &addressBar)
			{
				app.OpenURL(addressBarURL.url);
			}
			else if (activeWidget->textField->form)
			{
				SubmitForm(activeWidget->textField->form);
			}
			DeactivateWidget();*/
			return true;
		}
		else if (event.key == KEYCODE_ARROW_LEFT)
		{
			if (cursorPosition == -1)
			{
				MoveCursorPosition(node, strlen(data->buffer));
			}
			else if (cursorPosition > 0)
			{
				MoveCursorPosition(node, cursorPosition - 1);
			}
			return true;
		}
		else if (event.key == KEYCODE_ARROW_RIGHT)
		{
			if (cursorPosition == -1)
			{
				MoveCursorPosition(node, strlen(data->buffer));
			}
			else if (cursorPosition < strlen(data->buffer))
			{
				MoveCursorPosition(node, cursorPosition + 1);
			}
			return true;
		}
		else if (event.key == KEYCODE_END)
		{
			if (cursorPosition == -1)
			{
				MoveCursorPosition(node, strlen(data->buffer));
			}
			else
			{
				MoveCursorPosition(node, strlen(data->buffer));
			}
			return true;
		}
		else if (event.key == KEYCODE_HOME)
		{
			if (cursorPosition == -1)
			{
				MoveCursorPosition(node, strlen(data->buffer));
			}
			else
			{
				MoveCursorPosition(node, 0);
			}
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

void TextFieldNode::DrawCursor(DrawContext& context, Node* node, bool clear)
{
	TextFieldNode::Data* data = static_cast<TextFieldNode::Data*>(node->data);
	Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
	

	int x = node->anchor.x + 2;
	int height = font->glyphHeight;

	for (int n = 0; n < cursorPosition; n++)
	{
		if (!data->buffer[n])
			break;
		x += font->GetGlyphWidth(data->buffer[n]);
	}

	if (x >= node->anchor.x + node->size.x - 1)
	{
		return;
	}

	int y = node->anchor.y + 2;

	context.surface->VLine(context, x, y, height, clear ? 1 : 0);
}

void TextFieldNode::MoveCursorPosition(Node* node, int newPosition)
{
	Platform::input->HideMouse();

	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);
	DrawCursor(context, node, true);
	cursorPosition = newPosition;
	DrawCursor(context, node, false);

	Platform::input->ShowMouse();
}

void TextFieldNode::RedrawModified(Node* node, int position)
{
	DrawContext context;

	Platform::input->HideMouse();
	App::Get().pageRenderer.GenerateDrawContext(context, node);
	Draw(context, node);

	Platform::input->ShowMouse();
}
