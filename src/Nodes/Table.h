#ifndef _TABLE_H_
#define _TABLE_H

#include "../Node.h"


class TableCellNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(bool inIsHeader) : isHeader(inIsHeader), columnIndex(0), rowIndex(0), columnSpan(1), rowSpan(1), nextCell(nullptr) {}
		Node* node;
		bool isHeader;
		int columnIndex;
		int rowIndex;
		int columnSpan;
		int rowSpan;
		TableCellNode::Data* nextCell;
	};

	static Node* Construct(Allocator& allocator, bool isHeader);
	virtual void ApplyStyle(Node* node);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;
};

class TableRowNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data() : rowIndex(0), numCells(0), nextRow(nullptr), firstCell(nullptr) {}
		int rowIndex;
		int numCells;
		TableRowNode::Data* nextRow;
		TableCellNode::Data* firstCell;
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
};

class TableNode : public NodeHandler
{
public:
	class Data
	{
	public:
		struct ColumnInfo
		{
			int preferredWidth;
			int calculatedWidth;
		};
		enum State
		{
			GeneratingLayout,
			FinalisingLayout,
			FinishedLayout
		};

		Data() : state(GeneratingLayout), numColumns(0), numRows(0), cellSpacing(2), cellPadding(2), columns(nullptr), firstRow(nullptr), cells(nullptr) {}

		bool IsGeneratingLayout() { return state == GeneratingLayout; }
		bool HasGeneratedCellGrid() { return cells != nullptr;  }

		State state;
		int numColumns;
		int numRows;
		int cellSpacing;
		int cellPadding;
		int totalWidth;
		ColumnInfo* columns;
		TableRowNode::Data* firstRow;
		TableCellNode::Data** cells;
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;

};

#endif
