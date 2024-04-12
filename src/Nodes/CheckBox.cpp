#include <stdio.h>
#include "../Layout.h"
#include "../Memory/LinAlloc.h"
#include "../Draw/Surface.h"
#include "../DataPack.h"
#include "../App.h"
#include "CheckBox.h"


Node* CheckBoxNode::Construct(Allocator& allocator, const char* name, const char* value, bool isRadio, bool isChecked)
{
	name = allocator.AllocString(name);
	value = allocator.AllocString(value);

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
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + 1, node->size.x - 2, outlineColour);
		context.surface->HLine(context, node->anchor.x + 1, node->anchor.y + node->size.y - 3, node->size.x - 2, outlineColour);
		context.surface->VLine(context, node->anchor.x + 1, node->anchor.y + 2, node->size.y - 5, outlineColour);
		context.surface->VLine(context, node->anchor.x + node->size.x - 2, node->anchor.y + 2, node->size.y - 5, outlineColour);
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
						for (Node* n = App::Get().page.GetRootNode(); n; n = n->GetNextInTree())
						{
							if (n != node && n->type == Node::CheckBox)
							{
								CheckBoxNode::Data* otherData = static_cast<CheckBoxNode::Data*>(n->data);
								if (otherData && otherData->isRadio && otherData->name && !strcmp(otherData->name, data->name))
								{
									otherData->isChecked = false;
									n->Redraw();
								}
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
		}
		return true;
	}

	return false;
}