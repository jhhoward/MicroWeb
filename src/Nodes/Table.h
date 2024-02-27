#ifndef _TABLE_H_
#define _TABLE_H

#include "../Node.h"

class TableNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data()  {}
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

class TableRowNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() {}
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

class TableCellNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() {}
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

#endif
