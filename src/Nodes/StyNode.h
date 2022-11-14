#pragma once

#include <stdint.h>
#include "../Node.h"
#include "../Font.h"
#include "../Style.h"

class StyleNode : public NodeHandler
{
public:
	class Data
	{
	public:
		ElementStyleOverride styleOverride;
	};

	virtual void ApplyStyle(Node* node) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;

	static Node* Construct(Allocator& allocator);
	static Node* ConstructFontStyle(Allocator& allocator, FontStyle::Type fontStyle, int fontSize = -1);
	static Node* ConstructFontSize(Allocator& allocator, int fontSize);
	static Node* ConstructAlignment(Allocator& allocator, ElementAlignment::Type alignment);
};

