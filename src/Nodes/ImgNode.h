#ifndef _IMGNODE_H_
#define _IMGNODE_H_

#include "../Node.h"
#include "../Image/Image.h"

class ImageNode: public NodeHandler
{
public:
	enum State
	{
		WaitingToDownload,
		DeterminingFormat,
		DownloadingDimensions,
		FinishedDownloadingDimensions,
		DownloadingContent,
		FinishedDownloadingContent,
		ErrorDownloading
	};

	class Data
	{
	public:
		Data() : source(nullptr), altText(nullptr), state(WaitingToDownload), isMap(false) {}
		bool HasDimensions() { return image.width > 0 && image.height > 0; }
		bool AreDimensionsLocked() { return state == DownloadingContent || state == FinishedDownloadingContent || state == ErrorDownloading; }
		bool IsBrokenImageWithoutDimensions();
		Image image;
		const char* source;
		char* altText;
		State state;
		bool isMap;

		ExplicitDimension explicitWidth;
		ExplicitDimension explicitHeight;
	};

	static Node* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;

	virtual void LoadContent(Node* node, struct LoadTask& loadTask) override;
	virtual bool ParseContent(Node* node, char* buffer, size_t count) override;
	virtual void FinishContent(Node* node, struct LoadTask& loadTask) override;

	virtual bool CanPick(Node* node) override { return true; }

	void ImageLoadError(Node* node);
};

#endif
