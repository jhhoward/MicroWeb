#ifndef _RENDER_H_
#define _RENDER_H_

#include "Draw/Surface.h"
#include "Node.h"

class App;
class Node;
struct DrawContext;
struct Rect;

#define MAX_RENDER_QUEUE_SIZE 512

struct RenderQueue
{
	struct Item
	{
		Node* node;
		int upperClip, lowerClip;
	};

	RenderQueue()
	{
		Reset();
	}

	void Reset()
	{
		head = tail = 0;
	}
	int Size()
	{
		return tail - head;
	}
	RenderQueue::Item* Dequeue()
	{
		if (head < tail)
		{
			RenderQueue::Item* result = &items[head++];
			if (head == tail)
			{
				tail = head = 0;
			}
			return result;
		}
		return nullptr;
	}

	int head, tail;
	Item items[MAX_RENDER_QUEUE_SIZE];
};

class PageRenderer
{
public:
	PageRenderer(App& inApp);

	void Init();
	void Reset();
	void Update();

	void RefreshAll();
	void DrawAll(DrawContext& context, Node* node);
	
	void GenerateDrawContext(DrawContext& context, Node* node);

	void AddToQueue(Node* node, int upperClip, int lowerClip);

	void OnPageScroll(int scrollDelta);

	void MarkNodeLayoutComplete(Node* node);
	void MarkPageLayoutComplete();
	void MarkNodeDirty(Node* node);

	void InvertNode(Node* node);

	int GetVisiblePageHeight() { return visiblePageHeight; }
	bool IsRendering() { return renderQueue.Size() > 0; }

private:
	bool IsInRenderQueue(Node* node);

	void InitContext(DrawContext& context);
	void ClampContextToRect(DrawContext& context, Rect& rect);
	void FindOverlappingNodesInScreenRegion(int top, int bottom);

	bool DoesOverlapWithContext(Node* node, DrawContext& context);
	bool IsRenderableNode(Node* node);

	void AddToQueueIfVisible(Node* node);
	int GetDrawOffsetY();

	App& app;

	RenderQueue renderQueue;

	Node* lastCompleteNode;

	int visiblePageHeight;
};

#endif

