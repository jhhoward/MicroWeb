#pragma once

#include "../Node.h"

class ImageNode: public NodeHandler
{
public:
	class Data
	{
	public:
		Data(void* inImageData) : imageData(inImageData) {}
		void* imageData;
	};

	static Node* Construct(Allocator& allocator, void* imageData);
	virtual void Draw(Page& page, Node* element) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;
};
