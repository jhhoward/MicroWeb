#ifndef _FIELD_H_
#define _FIELD_H_

#include "../Node.h"

#define DEFAULT_TEXT_FIELD_BUFFER_SIZE 80

class TextFieldNode : public NodeHandler
{
public:
	TextFieldNode() :
		shiftPosition(0),
		cursorPosition(0),
		selectionStartPosition(0),
		selectionLength(0),
		pickedPosition(0)
	{

	}

	class Data
	{
	public:
		Data(char* inBuffer, int inBufferSize, NodeCallbackFunction inOnSubmit) : buffer(inBuffer), bufferSize(inBufferSize), name(NULL), onSubmit(inOnSubmit) {}
		char* buffer;
		int bufferSize;
		char* name;
		NodeCallbackFunction onSubmit;
		ExplicitDimension explicitWidth;
	};

	static Node* Construct(Allocator& allocator, const char* text, NodeCallbackFunction onSubmit = NULL);
	static Node* Construct(Allocator& allocator, char* buffer, int bufferLength, NodeCallbackFunction onSubmit = NULL);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;
	virtual bool CanPick(Node* node) override { return true; }
	virtual bool HandleEvent(Node* node, const Event& event) override;

private:
	int shiftPosition;
	int cursorPosition;
	int selectionStartPosition;
	int selectionLength;
	int pickedPosition;

	void ShiftIntoView(Node* node);
	void DrawCursor(DrawContext& context, Node* node);
	void DrawSelection(DrawContext& context, Node* node);
	void MoveCursorPosition(Node* node, int newPosition);
	void RedrawModified(Node* node, int position);
	int GetBufferPixelWidth(Node* node, int start, int end);
	void DeleteSelectionContents(Node* node);
	void ClearSelection(Node* node);
	int PickPosition(Node* node, int x, int y);
};

#endif
