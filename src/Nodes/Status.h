#ifndef _STATUS_H_
#define _STATUS_H_

#include "../Node.h"

#define MAX_STATUS_BAR_MESSAGE_LENGTH 100

class StatusBarNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() { message[0] = '\0'; }
		char message[MAX_STATUS_BAR_MESSAGE_LENGTH];
	};

	static Node* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* node) override;
};

#endif
