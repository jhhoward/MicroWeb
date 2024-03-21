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
	uint8_t outlineColour = Platform::video->colourScheme.textColour;

	if (data->state == ImageNode::FinishedDownloadingContent)
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
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	if (!data->AreDimensionsLocked() && data->image.width > layout.MaxAvailableWidth())
	{
		int imageWidth = data->image.width;
		data->image.width = layout.MaxAvailableWidth();
		data->image.height = ((long)data->image.height * data->image.width) / imageWidth;
	}

	node->size.x = data->image.width;
	node->size.y = data->image.height;

	if (layout.AvailableWidth() < node->size.x)
	{
		layout.BreakNewLine();
	}

	node->anchor = layout.GetCursor(node->size.y);
	layout.ProgressCursor(node, node->size.x, node->size.y);
}

void ImageNode::LoadContent(Node* node, LoadTask& loadTask)
{
	if (!App::Get().config.loadImages)
	{
		return;
	}

	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	if (data && data->state != ImageNode::ErrorDownloading && data->state != ImageNode::FinishedDownloadingContent) 
	{
		if (data->HasDimensions() && !App::Get().page.layout.IsFinished())
		{
			return;
		}

		if (!data->source)
		{
			data->state = ImageNode::ErrorDownloading;
		}
		else 
		{
			bool loadDimensionsOnly = !data->HasDimensions();
			if (!loadDimensionsOnly && App::Get().pageLoadTask.HasContent())
				return;
			loadTask.Load(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->source).url);
			ImageDecoder::Create(ImageDecoder::Gif);
			ImageDecoder::Get()->Begin(&data->image, loadDimensionsOnly);
			data->state = loadDimensionsOnly ? ImageNode::DownloadingDimensions : ImageNode::DownloadingContent;
		}
	}
}

bool ImageNode::ParseContent(Node* node, char* buffer, size_t count)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	ImageDecoder* decoder = ImageDecoder::Get();

	decoder->Process((uint8_t*) buffer, count);
	if (decoder->GetState() == ImageDecoder::Success)
	{
		if (data->state == ImageNode::DownloadingDimensions)
		{
			data->state = ImageNode::FinishedDownloadingDimensions;
		}
		else
		{
			data->state = ImageNode::FinishedDownloadingContent;
			App::Get().pageRenderer.MarkNodeDirty(node);
		}

		// Loop through image nodes in case this image is used multiple times
		for (Node* n = node; n; n = n->GetNextInTree())
		{
			if (n->type == Node::Image)
			{
				ImageNode::Data* otherData = static_cast<ImageNode::Data*>(n->data);

				if (otherData->source && !strcmp(data->source, otherData->source) && n != node)
				{
					if (!otherData->HasDimensions() || (otherData->image.width == data->image.width && otherData->image.height == data->image.height))
					{
						otherData->image = data->image;
						otherData->state = data->state;

						if (data->state == ImageNode::FinishedDownloadingContent)
						{
							App::Get().pageRenderer.MarkNodeDirty(n);
						}
					}
				}
			}
		}
	}
	else if (decoder->GetState() != ImageDecoder::Decoding)
	{
		data->state = ImageNode::ErrorDownloading;
		if (!data->HasDimensions())
		{
			data->image.width = data->image.height = 1;
		}
	}

	return decoder->GetState() == ImageDecoder::Decoding;
}
