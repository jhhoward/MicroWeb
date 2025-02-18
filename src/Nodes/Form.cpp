#include "Form.h"
#include "../Memory/Memory.h"
#include "../App.h"
#include "../Interface.h"
#include "Field.h"
#include "CheckBox.h"
#include "Select.h"

#include "../HTTP.h"

Node* FormNode::Construct(Allocator& allocator)
{
	FormNode::Data* data = allocator.Alloc<FormNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::Form, data);
	}

	return nullptr;
}

void FormNode::AppendParameter(char* address, const char* name, const char* value, int& numParams, size_t bufferLength)
{
	if (!name)
		return;

	if (numParams == 0)
	{
		strncat(address, "?", bufferLength);
	}
	else
	{
		strncat(address, "&", bufferLength);
	}
	strncat(address, name, bufferLength);
	strncat(address, "=", bufferLength);
	if (value)
	{
		strncat(address, value, bufferLength);
	}
	numParams++;

}

void FormNode::BuildAddressParameterList(Node* node, char* address, int& numParams, size_t bufferLength)
{
	switch(node->type)
	{
		case Node::TextField:
		{
			TextFieldNode::Data* fieldData = static_cast<TextFieldNode::Data*>(node->data);

			if (fieldData->name && fieldData->buffer)
			{
				AppendParameter(address, fieldData->name, fieldData->buffer, numParams, bufferLength);
			}
		}
		break;
		case Node::CheckBox:
		{
			CheckBoxNode::Data* checkboxData = static_cast<CheckBoxNode::Data*>(node->data);

			if (checkboxData && checkboxData->isChecked && checkboxData->name && checkboxData->value)
			{
				AppendParameter(address, checkboxData->name, checkboxData->value, numParams, bufferLength);
			}
		}
		break;
		case Node::Select:
		{
			SelectNode::Data* selectData = static_cast<SelectNode::Data*>(node->data);

			if (selectData && selectData->selected)
			{
				AppendParameter(address, selectData->name, selectData->selected->text, numParams, bufferLength);
			}
		}
		break;
	}

	for (node = node->firstChild; node; node = node->next)
	{
		BuildAddressParameterList(node, address, numParams, bufferLength);
	}
}

void FormNode::SubmitForm(Node* node)
{
	FormNode::Data* data = static_cast<FormNode::Data*>(node->data);
	App& app = App::Get();

	char* address = app.ui.addressBarURL.url;
	if (data->action)
	{
		strncpy(address, data->action, MAX_URL_LENGTH);
	}
	int numParams = 0;

	// Remove anything after existing ?
	char* questionMark = strstr(address, "?");
	if (questionMark)
	{
		*questionMark = '\0';
	}

	char* paramStart = address + strlen(address);

	BuildAddressParameterList(node, address, numParams, MAX_URL_LENGTH);

	// Replace any spaces with +
	for (char* p = paramStart; *p; p++)
	{
		if (*p == ' ')
		{
			*p = '+';
		}
	}

	if (data->method == FormNode::Data::Post)
	{
		static HTTPOptions postOptions;

		postOptions.contentData = paramStart + 1;
		postOptions.keepAlive = false;
		postOptions.headerParams = NULL;
		postOptions.postContentType = "application/x-www-form-urlencoded";

		// Remove '?' from URL as params are passed as part of the POST request
		*paramStart = '\0';

		app.OpenURL(HTTPRequest::Post, URL::GenerateFromRelative(app.page.pageURL.url, address).url, &postOptions);
	}
	else
	{
		app.OpenURL(HTTPRequest::Get, URL::GenerateFromRelative(app.page.pageURL.url, address).url);
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
