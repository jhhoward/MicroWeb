#ifndef _CHECKBOX_H_
#define _CHECKBOX_H_

#include "../Node.h"

class CheckBoxNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(const char* inName, const char* inValue, bool inIsRadio, bool inIsChecked) :
			name(inName), value(inValue), isRadio(inIsRadio), isChecked(inIsChecked) {}
		const char* name;
		const char* value;
		bool isRadio;
		bool isEnabled;
		bool isChecked;
	};

	static Node* Construct(Allocator& allocator, const char* name, const char* value, bool isRadio, bool isChecked);
	virtual void Draw(DrawContext& context, Node* element) override;
	virtual void GenerateLayout(Layout& layout, Node* node) override;
	virtual bool CanPick(Node* node) { return true; }
	virtual bool HandleEvent(Node* node, const Event& event);
};

#endif
