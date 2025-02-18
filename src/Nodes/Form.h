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

	static void SubmitForm(Node* node);

	static void OnSubmitButtonPressed(Node* node);

private:
	static void BuildAddressParameterList(Node* node, char* address, int& numParams, size_t bufferLength);
	static void AppendParameter(char* address, const char* name, const char* value, int& numParams, size_t bufferLength);
};

#endif
