#include "../Platform.h"
#include "../Page.h"
#include "ImgNode.h"
#include "../Memory/Memory.h"
#include "../Layout.h"
#include "../Draw/Surface.h"
#include "../App.h"
#include "../Image/Decoder.h"
#include "../DataPack.h"
#include "../HTTP.h"
#include "Text.h"

void ImageNode::Draw(DrawContext& context, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);
	//printf("--IMG [%d, %d]\n", node->anchor.x, node->anchor.y);
	uint8_t outlineColour = Platform::video->colourScheme.textColour;

	if (data->state == ImageNode::FinishedDownloadingContent && data->image.lines.IsAllocated())
	{
		context.surface->BlitImage(context, &data->image, node->anchor.x, node->anchor.y);
	}
	else
	{
		Image* image = data->state == ImageNode::ErrorDownloading ? Assets.brokenImageIcon : Assets.imageIcon;

		if (data->IsBrokenImageWithoutDimensions())
		{
			context.surface->BlitImage(context, image, node->anchor.x, node->anchor.y);
		}
		else
		{
			context.surface->HLine(context, node->anchor.x, node->anchor.y, node->size.x, outlineColour);
			context.surface->HLine(context, node->anchor.x, node->anchor.y + node->size.y - 1, node->size.x, outlineColour);
			context.surface->VLine(context, node->anchor.x, node->anchor.y + 1, node->size.y - 2, outlineColour);
			context.surface->VLine(context, node->anchor.x + node->size.x - 1, node->anchor.y + 1, node->size.y - 2, outlineColour);

			DrawContext croppedContext = context;
			croppedContext.Restrict(node->anchor.x + 1, node->anchor.y + 1, node->anchor.x + node->size.x - 1, node->anchor.y + node->size.y - 1);
			croppedContext.surface->BlitImage(croppedContext, image, node->anchor.x + 2, node->anchor.y + 2);

			if (data->altText)
			{
				Font* font = Assets.GetFont(node->style.fontSize, node->style.fontStyle);
				uint8_t textColour = Platform::video->colourScheme.textColour;
				croppedContext.surface->DrawString(croppedContext, font, data->altText, node->anchor.x + image->width + 4, node->anchor.y + 2, textColour, node->style.fontStyle);
			}
		}
	}

	Node* focusedNode = App::Get().ui.GetFocusedNode();
	if (focusedNode && node->IsChildOf(focusedNode))
	{
		context.surface->InvertRect(context, node->anchor.x, node->anchor.y, node->size.x, node->size.y);
	}
}

bool ImageNode::Data::IsBrokenImageWithoutDimensions()
{
	return state == ImageNode::ErrorDownloading && image.width == Assets.brokenImageIcon->width && image.height == Assets.brokenImageIcon->height;
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

void ImageNode::BeginLayoutContext(Layout& layout, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	if (!data->AreDimensionsLocked())
	{
		if (data->explicitWidth.IsSet())
		{
			data->image.width = layout.CalculateWidth(data->explicitWidth);
		}
		if(data->explicitHeight.IsSet())
		{
			data->image.height = layout.CalculateHeight(data->explicitHeight);
		}
	}
}

void ImageNode::GenerateLayout(Layout& layout, Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	if (!data->AreDimensionsLocked())
	{
		if (data->image.width > layout.MaxAvailableWidth())
		{
			int imageWidth = data->image.width;
			data->image.width = layout.MaxAvailableWidth();
			data->image.height = (uint16_t)(((long)data->image.height * data->image.width) / imageWidth);
		}
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
			ImageLoadError(node);
		}
		else 
		{
			bool loadDimensionsOnly = !data->HasDimensions();
			if (!loadDimensionsOnly && App::Get().pageLoadTask.HasContent())
				return;

			loadTask.Load(URL::GenerateFromRelative(App::Get().page.pageURL.url, data->source).url);
			data->state = ImageNode::DeterminingFormat;
		}
	}
}

void ImageNode::FinishContent(Node* node, struct LoadTask& loadTask) 
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	if (data)
	{
		switch (data->state)
		{
		case ImageNode::DownloadingDimensions:
		case ImageNode::DownloadingContent:
		case ImageNode::DeterminingFormat:
			ImageLoadError(node);
			break;
		}
	}
}

void ImageNode::ImageLoadError(Node* node)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	if (data->state != ImageNode::ErrorDownloading)
	{
		if (!data->HasDimensions())
		{
			data->image.width = Assets.brokenImageIcon->width;
			data->image.height = Assets.brokenImageIcon->height;

			if (data->altText)
			{
				Node* altTextNode = TextElement::Construct(MemoryManager::pageAllocator, data->altText);
				if (altTextNode)
				{
					node->InsertSibling(altTextNode);
					altTextNode->Handler().ApplyStyle(altTextNode);
				}
			}
		}
		data->state = ImageNode::ErrorDownloading;
	}
}


bool ImageNode::ParseContent(Node* node, char* buffer, size_t count)
{
	ImageNode::Data* data = static_cast<ImageNode::Data*>(node->data);

	if (data->state == ImageNode::DeterminingFormat)
	{
		bool loadDimensionsOnly = !data->HasDimensions();
		LoadTask& loadTask = App::Get().pageContentLoadTask;
		
		if((loadTask.type == LoadTask::RemoteFile && ImageDecoder::CreateFromMIME(loadTask.request->GetContentType()))
			|| ImageDecoder::CreateFromExtension(data->source))
		{
			ImageDecoder::Get()->Begin(&data->image, loadDimensionsOnly);
			data->state = loadDimensionsOnly ? ImageNode::DownloadingDimensions : ImageNode::DownloadingContent;
		}
		else
		{
			// Unsupported image format
			ImageLoadError(node);
			return false;
		}
	}

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
		ImageLoadError(node);
	}

	return decoder->GetState() == ImageDecoder::Decoding;
}
