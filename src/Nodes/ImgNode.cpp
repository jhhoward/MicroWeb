#include "../Platform.h"
#include "../Page.h"
#include "ImgNode.h"
#include "../LinAlloc.h"
#include "../Layout.h"
#include "../Draw/Surface.h"
#include "../App.h"
#include "../Image/Decoders.h"

void ImageNode::Draw(DrawContext& context, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	//printf("--IMG [%d, %d]\n", node->anchor.x, node->anchor.y);
	uint8_t outlineColour = 0;

	if (data->image.data)
	{
		context.surface->BlitImage(context, &data->image, node->anchor.x, node->anchor.y);
	}
	else
	{
		context.surface->HLine(context, node->anchor.x, node->anchor.y, node->size.x, outlineColour);
		context.surface->HLine(context, node->anchor.x, node->anchor.y + node->size.y - 1, node->size.x, outlineColour);
		context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, outlineColour);
		context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, outlineColour);
	}
}


Node* ImageNode::Construct(Allocator& allocator)
{
	ImageNode::Data* data = allocator.Alloc<ImageNode::Data>();
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

void ImageNode::LoadContent(Node* node, LoadTask& loadTask)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	if (data && data->source)
	{
		loadTask.Load(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->source).url);
		ImageDecoders.Create<GifDecoder>(App::Get().page.allocator);
		ImageDecoders.Get()->Begin(&data->image);
	}
}

bool ImageNode::ParseContent(Node* node, char* buffer, size_t count)
{
	ImageDecoder* decoder = ImageDecoders.Get();
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	decoder->Process((uint8_t*) buffer, count);
	if (decoder->GetState() == ImageDecoder::Success)
	{
		node->size.x = data->image.width;
		node->size.y = data->image.height;
		App::Get().page.layout.RecalculateLayout();
		printf("Success!\n");
	}

	return decoder->GetState() == ImageDecoder::Decoding;
}
