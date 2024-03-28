#ifndef _STATUS_H_
#define _STATUS_H_

#include "../Node.h"

#define STATUS_MESSAGE_BUFFER_SIZE 100
#define MAX_STATUS_BAR_MESSAGE_LENGTH (STATUS_MESSAGE_BUFFER_SIZE - 4)

class StatusBarNode : public NodeHandler
{
public:
	enum StatusType
	{
		GeneralStatus,
		HoverStatus,
		NumStatusTypes
	};

	class Data
	{
	public:
		struct Message
		{
			Message() 
			{ 
				message[0] = '\0';
				message[STATUS_MESSAGE_BUFFER_SIZE - 1] = '\0';
				message[STATUS_MESSAGE_BUFFER_SIZE - 2] = '.';
				message[STATUS_MESSAGE_BUFFER_SIZE - 3] = '.';
				message[STATUS_MESSAGE_BUFFER_SIZE - 4] = '.';
			}
			void Clear() { message[0] = '\0'; }
			bool HasMessage() { return message[0] != '\0'; }

			char message[STATUS_MESSAGE_BUFFER_SIZE];
		};
		Message messages[NumStatusTypes];
	};

	static Node* Construct(Allocator& allocator);
	virtual void Draw(DrawContext& context, Node* node) override;

	static void SetStatus(Node* node, const char* message, StatusType type);
};

#endif
