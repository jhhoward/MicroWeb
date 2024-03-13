#ifndef _RENDER_H_
#define _RENDER_H_

#include "Draw/Surface.h"
#include "Node.h"

class App;
class Node;
struct DrawContext;
struct Rect;

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

	void AddToQueue(Node* node);

	void OnPageScroll(int scrollDelta);

	void MarkNodeLayoutComplete(Node* node);
	void MarkPageLayoutComplete();
	void MarkNodeDirty(Node* node);

	int GetVisiblePageHeight() { return visiblePageHeight; }

private:
	bool IsInRenderQueue(Node* node);

	void InitContext(DrawContext& context);
	void ClampContextToRect(DrawContext& context, Rect& rect);
	void FindOverlappingNodesInScreenRegion(int top, int bottom);

	bool DoesOverlapWithContext(Node* node, DrawContext& context);
	bool IsRenderableNode(Node::Type type);

	void AddToQueueIfVisible(Node* node);

	App& app;

	Node* renderQueueHead;
	Node* renderQueueTail;

	int renderQueueSize;

	Node* lastCompleteNode;

	// Draw contexts for upper and lower parts of the screen to facilitate scrolling
	DrawContext upperContext;
	DrawContext lowerContext;

	int visiblePageHeight;
};

#endif

