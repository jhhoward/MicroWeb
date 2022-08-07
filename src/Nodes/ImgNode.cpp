#include "../Platform.h"
#include "../Page.h"
#include "ImgNode.h"
#include "../LinAlloc.h"
#include "../Layout.h"

void ImageNode::Draw(Page& page, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	printf("--IMG [%d, %d]\n", node->anchor.x, node->anchor.y);
	Platform::video->FillRect(node->anchor.x, node->anchor.y + 50, node->size.x, node->size.y);
}


Node* ImageNode::Construct(Allocator& allocator, void* imageData)
{
	ImageNode::Data* data = allocator.Alloc<ImageNode::Data>(imageData);
	if (data)
	{
		return allocator.Alloc<Node>(Node::Image, data);
	}

	return nullptr;
}

void ImageNode::GenerateLayout(Layout& layout, Node* node)
{
	int imageWidth = node->size.x;
	int imageHeight = node->size.y;
	if (layout.AvailableWidth() < imageWidth)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(imageHeight);
	layout.ProgressCursor(node, imageWidth, imageHeight);
}
