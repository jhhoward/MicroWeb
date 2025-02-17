#include <stdio.h>
#include "../Layout.h"
#include "../Memory/LinAlloc.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"
#include "../App.h"
#include "CheckBox.h"
#include "../KeyCodes.h"

Node* CheckBoxNode::Construct(Allocator& allocator, const char* name, const char* value, bool isRadio, bool isChecked)
{
	name = name ? allocator.AllocString(name) : NULL;
	value = value ? allocator.AllocString(value) : NULL;

	CheckBoxNode::Data* data = allocator.Alloc<CheckBoxNode::Data>(name, value, isRadio, isChecked);
	if (data)
	{
		return allocator.Alloc<Node>(Node::CheckBox, data);
	}
	return nullptr;
}

void CheckBoxNode::Draw(DrawContext& context, Node* node)
{
	CheckBoxNode::Data* data = static_cast<CheckBoxNode::Data*>(node->data);

	if (data)
	{
		if (data->isRadio)
		{
			if (data->isChecked)
			{
				context.surface->BlitImage(context, Assets.radioSelected, node->anchor.x, node->anchor.y);
			}
			else
			{
				context.surface->BlitImage(context, Assets.radio, node->anchor.x, node->anchor.y);
			}
		}
		else
		{
			if (data->isChecked)
			{
				context.surface->BlitImage(context, Assets.checkboxTicked, node->anchor.x, node->anchor.y);
			}
			else
			{
				context.surface->BlitImage(context, Assets.checkbox, node->anchor.x, node->anchor.y);
			}
		}
	}

	if (App::Get().ui.GetFocusedNode() == node)
	{
		uint8_t outlineColour = Platform::video->colourScheme.textColour;

		context.surface->HLine(context, node->anchor.x, node->anchor.y, node->size.x, outlineColour);
		context.surface->HLine(context, node->anchor.x, node->anchor.y + node->size.y - 1, node->size.x, outlineColour);
		context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, outlineColour);
		context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, outlineColour);
	}

}


void CheckBoxNode::GenerateLayout(Layout& layout, Node* node)
{
	CheckBoxNode::Data* data = static_cast<CheckBoxNode::Data*>(node->data);
	Image* image = data->isRadio ? Assets.radio : Assets.checkbox;

	Font* font = node->GetStyleFont();

	node->size.x = image->width;
	node->size.y = image->height;

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

Node* CheckBoxNode::FindPreviousRadioNode(Node* node)
{
	Node* n = node;
	Node* next = FindNextRadioNode(node);

	while (next != node)
	{
		n = next;
		next = FindNextRadioNode(n);
	}

	return n;
}

Node* CheckBoxNode::FindNextRadioNode(Node* node)
{
	for (Node* n = node->GetNextInTree(); n; n = n->GetNextInTree())
	{
		if (IsPartOfRadioSet(node, n))
		{
			return n;
		}
	}

	for (Node* n = App::Get().page.GetRootNode(); n && n != node; n = n->GetNextInTree())
	{
		if (IsPartOfRadioSet(node, n))
		{
			return n;
		}
	}

	return node;
}

bool CheckBoxNode::IsPartOfRadioSet(Node* contextNode, Node* node)
{
	if (node->type == Node::CheckBox)
	{
		Node* formNode = contextNode->FindParentOfType(Node::Form);
		CheckBoxNode::Data* contextData = static_cast<CheckBoxNode::Data*>(contextNode->data);

		CheckBoxNode::Data* data = static_cast<CheckBoxNode::Data*>(node->data);
		Node* otherFormNode = node->FindParentOfType(Node::Form);

		if (data && data->isRadio && formNode == otherFormNode && data->name && !strcmp(contextData->name, data->name))
		{
			return true;
		}
	}

	return false;
}

bool CheckBoxNode::HandleEvent(Node* node, const Event& event)
{
	AppInterface& ui = App::Get().ui;
	CheckBoxNode::Data* data = static_cast<CheckBoxNode::Data*>(node->data);

	switch (event.type)
	{
		case Event::MouseClick:
		{
			if (data->isRadio)
			{
				if (!data->isChecked)
				{
					data->isChecked = true;

					if (data->name)
					{
						Node* formNode = node->FindParentOfType(Node::Form);

						for (Node* n = App::Get().page.GetRootNode(); n; n = n->GetNextInTree())
						{
							if (n != node && IsPartOfRadioSet(node, n))
							{
								CheckBoxNode::Data* otherData = static_cast<CheckBoxNode::Data*>(n->data);
								otherData->isChecked = false;
								n->Redraw();
							}
						}
					}
				}
			}
			else
			{
				data->isChecked = !data->isChecked;
			}
			node->Redraw();
			ui.FocusNode(nullptr);
		}
		return true;

		case Event::Focus:
		{
			bool shouldFocus = !data->isRadio || data->isChecked;

			if (!shouldFocus)
			{
				shouldFocus = true;

				// If this is a radio type and no option is selected, allow focus
				for (Node* n = App::Get().page.GetRootNode(); n; n = n->GetNextInTree())
				{
					if (IsPartOfRadioSet(node, n))
					{
						CheckBoxNode::Data* otherData = static_cast<CheckBoxNode::Data*>(n->data);
						if (otherData->isChecked)
						{
							shouldFocus = false;
							break;
						}
					}
				}
			}

			if (shouldFocus)
			{
				DrawHighlight(node, Platform::video->colourScheme.textColour);
				return true;
			}
			else
			{
				return false;
			}
		}

		case Event::Unfocus:
		DrawHighlight(node, Platform::video->colourScheme.pageColour);
		return true;

		case Event::KeyPress:
			if (data->isRadio)
			{
				switch (event.key)
				{
				case KEYCODE_ARROW_UP:
				case KEYCODE_ARROW_LEFT:
					{
						Node* next = FindPreviousRadioNode(node);
						if (next && next != node)
						{
							data->isChecked = false;
							CheckBoxNode::Data* nextData = static_cast<CheckBoxNode::Data*>(next->data);
							nextData->isChecked = true;
							node->Redraw();
							next->Redraw();
							ui.FocusNode(next);
						}
					}
					return true;
				case KEYCODE_ARROW_DOWN:
				case KEYCODE_ARROW_RIGHT:
					{
						Node* next = FindNextRadioNode(node);
						if (next && next != node)
						{
							data->isChecked = false;
							CheckBoxNode::Data* nextData = static_cast<CheckBoxNode::Data*>(next->data);
							nextData->isChecked = true;
							node->Redraw();
							next->Redraw();
							ui.FocusNode(next);
						}
					}
					return true;
				case ' ':
					if (!data->isChecked)
					{
						data->isChecked = true;
						node->Redraw();
					}
					return true;
				}
			}
			else
			{
				switch (event.key)
				{
				case ' ':
					data->isChecked = !data->isChecked;
					node->Redraw();
					return true;
				}
			}
		break;
	}

	return false;
}

void CheckBoxNode::DrawHighlight(Node* node, uint8_t colour)
{
	DrawContext context;
	App::Get().pageRenderer.GenerateDrawContext(context, node);

	Platform::input->HideMouse();
	context.surface->HLine(context, node->anchor.x, node->anchor.y, node->size.x, colour);
	context.surface->HLine(context, node->anchor.x, node->anchor.y + node->size.y - 1, node->size.x, colour);
	context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, colour);
	context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, colour);
	Platform::input->ShowMouse();
}
