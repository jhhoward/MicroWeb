#include "Form.h"
#include "../Memory/Memory.h"
#include "../App.h"
#include "../Interface.h"
#include "Field.h"

Node* FormNode::Construct(Allocator& allocator)
{
	FormNode::Data* data = allocator.Alloc<FormNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Form, data);
	}

	return nullptr;
}

void FormNode::BuildAddressParameterList(Node* node, char* address, int& numParams)
{
	if (node->type == Node::TextField)
	{
		TextFieldNode::Data* fieldData = static_cast<TextFieldNode::Data*>(node->data);

		if (fieldData->name && fieldData->buffer)
		{
			if (numParams == 0)
			{
				strcat(address, "?");
			}
			else
			{
				strcat(address, "&");
			}
			strcat(address, fieldData->name);
			strcat(address, "=");
			strcat(address, fieldData->buffer);
			numParams++;
		}
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
		strcpy(address, data->action);
		int numParams = 0;

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
