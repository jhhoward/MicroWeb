#include "Form.h"
#include "../Memory/Memory.h"
#include "../App.h"
#include "../Interface.h"
#include "Field.h"
#include "CheckBox.h"
#include "Select.h"

Node* FormNode::Construct(Allocator& allocator)
{
	FormNode::Data* data = allocator.Alloc<FormNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Form, data);
	}

	return nullptr;
}

void FormNode::AppendParameter(char* address, const char* name, const char* value, int& numParams)
{
	if (!name)
		return;

	if (numParams == 0)
	{
		strcat(address, "?");
	}
	else
	{
		strcat(address, "&");
	}
	strcat(address, name);
	strcat(address, "=");
	if (value)
	{
		strcat(address, value);
	}
	numParams++;

}

void FormNode::BuildAddressParameterList(Node* node, char* address, int& numParams)
{
	switch(node->type)
	{
		case Node::TextField:
		{
			TextFieldNode::Data* fieldData = static_cast<TextFieldNode::Data*>(node->data);

			if (fieldData->name && fieldData->buffer)
			{
				AppendParameter(address, fieldData->name, fieldData->buffer, numParams);
			}
		}
		break;
		case Node::CheckBox:
		{
			CheckBoxNode::Data* checkboxData = static_cast<CheckBoxNode::Data*>(node->data);

			if (checkboxData && checkboxData->isChecked && checkboxData->name && checkboxData->value)
			{
				AppendParameter(address, checkboxData->name, checkboxData->value, numParams);
			}
		}
		break;
		case Node::Select:
		{
			SelectNode::Data* selectData = static_cast<SelectNode::Data*>(node->data);

			if (selectData && selectData->selected)
			{
				AppendParameter(address, selectData->name, selectData->selected->text, numParams);
			}
		}
		break;
	}

	for (node = node->firstChild; node; node = node->next)
	{
		BuildAddressParameterList(node, address, numParams);
	}
}

void FormNode::SubmitForm(Node* node)
{
	FormNode::Data* data = static_cast<FormNode::Data*>(node->data);
	App& app = App::Get();

	if (data->method == FormNode::Data::Get)
	{
		char* address = app.ui.addressBarURL.url;
		if (data->action)
		{
			strcpy(address, data->action);
		}
		int numParams = 0;

		// Remove anything after existing ?
		char* questionMark = strstr(address, "?");
		if (questionMark)
		{
			*questionMark = '\0';
		}

		BuildAddressParameterList(node, address, numParams);

		// Replace any spaces with +
		for (char* p = address; *p; p++)
		{
			if (*p == ' ')
			{
				*p = '+';
			}
		}

		app.OpenURL(URL::GenerateFromRelative(app.page.pageURL.url, address).url);
	}
}

void FormNode::OnSubmitButtonPressed(Node* node)
{
	Node* formNode = node->FindParentOfType(Node::Form);
	if (formNode)
	{
		SubmitForm(formNode);
	}
}
