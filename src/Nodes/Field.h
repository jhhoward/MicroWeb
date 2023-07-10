#ifndef _FIELD_H_
#define _FIELD_H_

#include "../Node.h"

#define DEFAULT_TEXT_FIELD_BUFFER_SIZE 80

class TextFieldNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(char* inBuffer, int inBufferSize) : buffer(inBuffer), bufferSize(inBufferSize), name(NULL) {}
		char* buffer;
		int bufferSize;
		char* name;
	};

	static Node* Construct(Allocator& allocator, const char* buttonText);
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;
	virtual bool CanPick(Node* node) { return true; }
};

#endif
