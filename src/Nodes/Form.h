#pragma once
#ifndef _FORM_H_
#define _FORM_H_


#include <stdint.h>
#include "../Node.h"

class FormNode : public NodeHandler
{
public:
	class Data
	{
	public:
		enum MethodType
		{
			Get,
			Post
		};
		char* action;
		MethodType method;

		Data() : action(NULL), method(Get) {}
	};

	static Node* Construct(Allocator& allocator);

	static void SubmitForm(App& app, Node* node);

	static void OnSubmitButtonPressed(App& app, Node* node);

private:
	static void BuildAddressParameterList(Node* node, char* address, int& numParams);
};

#endif
