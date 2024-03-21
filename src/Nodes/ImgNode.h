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
		DownloadingDimensions,
		FinishedDownloadingDimensions,
		DownloadingContent,
		FinishedDownloadingContent,
		ErrorDownloading
	};

	class Data
	{
	public:
		Data() : source(nullptr), state(WaitingToDownload) {}
		bool HasDimensions() { return image.width > 0; }
		bool AreDimensionsLocked() { return state == DownloadingContent || state == FinishedDownloadingContent; }
		Image image;
		const char* source;
		State state;
	};

	static Node* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;

	virtual void LoadContent(Node* node, struct LoadTask& loadTask) override;
	virtual bool ParseContent(Node* node, char* buffer, size_t count) override;

};

#endif
