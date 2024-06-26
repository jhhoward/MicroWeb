#ifndef _TABLE_H_
#define _TABLE_H

#include "../Node.h"
#include "../Colour.h"

class TableCellNode : public NodeHandler
{
public:
	class Data
	{
	public:
		Data(bool inIsHeader) : node(nullptr), isHeader(inIsHeader), columnIndex(0), rowIndex(0), columnSpan(1), rowSpan(1), bgColour(TRANSPARENT_COLOUR_VALUE), nextCell(nullptr) {}
		Node* node;
		bool isHeader;
		int columnIndex;
		int rowIndex;
		int columnSpan;
		int rowSpan;
		uint8_t bgColour;
		TableCellNode::Data* nextCell;
		ExplicitDimension explicitWidth;
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
		Data() : node(nullptr), rowIndex(0), numCells(0), nextRow(nullptr), firstCell(nullptr) {}
		Node* node;
		int rowIndex;
		int numCells;
		TableRowNode::Data* nextRow;
		TableCellNode::Data* firstCell;
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
	virtual void ApplyStyle(Node* node);
};

class TableNode : public NodeHandler
{
public:
	class Data
	{
	public:
		struct ColumnInfo
		{
			void Clear()
			{
				preferredWidth = 0;
				calculatedWidth = 0;
				explicitWidthPixels = 0;
				explicitWidthPercentage = 0;
			}
			int preferredWidth;
			int calculatedWidth;
			int explicitWidthPixels;
			int explicitWidthPercentage;
		};
		enum State
		{
			GeneratingLayout,
			FinalisingLayout,
			FinishedLayout
		};

		Data() : state(GeneratingLayout), numColumns(0), numRows(0), cellSpacing(2), cellPadding(2), border(0), columns(nullptr), firstRow(nullptr), cells(nullptr), bgColour(TRANSPARENT_COLOUR_VALUE), lastAvailableWidth(-1) {}

		bool IsGeneratingLayout() { return state == GeneratingLayout; }
		bool HasGeneratedCellGrid() { return cells != nullptr;  }

		State state;
		int numColumns;
		int numRows;
		int cellSpacing;
		int cellPadding;
		uint8_t border;
		int totalWidth;
		ColumnInfo* columns;
		TableRowNode::Data* firstRow;
		TableCellNode::Data** cells;
		uint8_t bgColour;
		int lastAvailableWidth;
		ExplicitDimension explicitWidth;
	};

	static Node* Construct(Allocator& allocator);
	virtual void BeginLayoutContext(Layout& layout, Node* node) override;
	virtual void EndLayoutContext(Layout& layout, Node* node) override;
	virtual void Draw(DrawContext& context, Node* node) override;

};

#endif
