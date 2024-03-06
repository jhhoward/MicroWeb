#include "../Platform.h"
#include "../Page.h"
#include "ImgNode.h"
#include "../Memory/Memory.h"
#include "../Layout.h"
#include "../Draw/Surface.h"
#include "../App.h"
#include "../Image/Decoder.h"

void ImageNode::Draw(DrawContext& context, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	//printf("--IMG [%d, %d]\n", node->anchor.x, node->anchor.y);
	uint8_t outlineColour = 7;

	if (data->image.lines)
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
	if (data && data->source && !data->image.lines)
	{
		loadTask.Load(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->source).url);
		ImageDecoder::Create(ImageDecoder::Gif);
		ImageDecoder::Get()->Begin(&data->image);
	}
}

bool ImageNode::ParseContent(Node* node, char* buffer, size_t count)
{
	ImageDecoder* decoder = ImageDecoder::Get();

	decoder->Process((uint8_t*) buffer, count);
	if (decoder->GetState() == ImageDecoder::Success)
	{
		ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

		// Loop through image nodes in case this image is used multiple times
		bool needsLayoutRefresh = false;
		for (Node* n = node; n; n = n->GetNextInTree())
		{
			if (n->type == Node::Image)
			{
				ImageNode::Data* otherData = static_cast<ImageNode::Data*>(n->data);

				if (otherData->source && !strcmp(data->source, otherData->source))
				{
					if (n != node)
					{
						otherData->image = data->image;
					}

					if (n->size.x != data->image.width || n->size.y != data->image.height)
					{
						n->size.x = data->image.width;
						n->size.y = data->image.height;
						needsLayoutRefresh = true;
					}
				}
			}
		}

		if (needsLayoutRefresh)
		{
			node->size.x = data->image.width;
			node->size.y = data->image.height;
			App::Get().page.layout.RecalculateLayout();
		}
		//printf("Success!\n");
	}

	return decoder->GetState() == ImageDecoder::Decoding;
}
