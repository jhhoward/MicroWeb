#pragma once

#include "../Node.h"
#include "../Font.h"

class StyleNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() : fontSize(1) { }
		int fontSize;
		FontStyle::Type fontStyle;
	};

	static Node* Construct(Allocator& allocator);
};

