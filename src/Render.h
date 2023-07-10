#ifndef _RENDER_H_
#define _RENDER_H_

class App;
class Node;
struct DrawContext;

class PageRenderer
{
public:
	PageRenderer(App& inApp);

	void Init();
	void Reset();
	void Update();

	void ScrollRelative(int delta);
	void ScrollAbsolute(int position);

	void DrawAll(DrawContext& context, Node* node);

	int GetScrollPositionY() { return scrollPositionY; }
	
	void GenerateDrawContext(DrawContext& context, Node* node);

private:
	App& app;
	int scrollPositionY;
};

#endif
