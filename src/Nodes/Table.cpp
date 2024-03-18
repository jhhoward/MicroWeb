#include <stdio.h>
#include "../Layout.h"
#include "../Memory/Memory.h"
#include "../Draw/Surface.h"
#include "Table.h"

/*
 * Table layout generation is done in 2 passes:
 * 1) Each cell has its content generated with the maximum available width to
 *    work out the preferred size
 * 2) Column and row dimensions are calculated based on preferred sizes
 */

Node* TableNode::Construct(Allocator& allocator)
{
	TableNode::Data* data = allocator.Alloc<TableNode::Data>();
	if (data)
	{
		data->cellPadding = 2;
		data->cellSpacing = 2;
		return allocator.Alloc<Node>(Node::Table, data);
	}
	return nullptr;
}

void TableNode::Draw(DrawContext& context, Node* node)
{
	TableNode::Data* data = static_cast<TableNode::Data*>(node->data);

	int x = node->anchor.x;
	int y = node->anchor.y;
	int w = node->size.x;
	int h = node->size.y;
	bool needsFill = data->bgColour != TRANSPARENT_COLOUR_VALUE && context.surface->bpp > 1;

	if (data->border)
	{
		uint8_t borderColour = Platform::video->colourScheme.textColour;
		context.surface->HLine(context, x, y, w, borderColour);
		context.surface->HLine(context, x, y + h - 1, w, borderColour);
		context.surface->VLine(context, x, y + 1, h - 2, borderColour);
		context.surface->VLine(context, x + w - 1, y + 1, h - 2, borderColour);

		if (needsFill)
		{
			context.surface->FillRect(context, x + 1, y + 1, w - 2, h - 2, data->bgColour);
		}
	}
	else if (needsFill)
	{
		context.surface->FillRect(context, x, y, w, h, data->bgColour);
	}
}

void TableNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableNode::Data* data = static_cast<TableNode::Data*>(node->data);

	layout.BreakNewLine();
	layout.tableDepth++;
	layout.PushCursor();
	layout.PushLayout();

	node->anchor = layout.Cursor();
	int availableWidth = layout.AvailableWidth();

	if (data->state == Data::FinishedLayout)
	{
		//if (availableWidth != data->lastAvailableWidth)
		{
			data->state = Data::GeneratingLayout;
		}
	}

	data->lastAvailableWidth = availableWidth;

	if (!data->IsGeneratingLayout())
	{
		layout.PadHorizontal(data->cellSpacing, data->cellSpacing);
		if (data->numRows > 0)
		{
			layout.PadVertical(data->cellSpacing);
		}
	}
}

void TableNode::EndLayoutContext(Layout& layout, Node* node)
{
	TableNode::Data* data = static_cast<TableNode::Data*>(node->data);

	layout.PopLayout();
	layout.PopCursor();

	if (data->IsGeneratingLayout())
	{
		if (!data->HasGeneratedCellGrid())
		{
			// Calculate number of rows and columns
			data->numRows = data->numColumns = 0;

			for (TableRowNode::Data* row = data->firstRow; row; row = row->nextRow)
			{
				int columnCount = 0;
				for (TableCellNode::Data* cell = row->firstCell; cell; cell = cell->nextCell)
				{
					columnCount += cell->columnSpan;
				}
				data->numRows++;

				if (columnCount > data->numColumns)
				{
					data->numColumns = columnCount;
				}
			}


			if (!data->cells)
			{
				data->cells = (TableCellNode::Data**)MemoryManager::pageAllocator.Allocate(sizeof(TableCellNode::Data*) * data->numRows * data->numColumns);
			}

			if (!data->columns)
			{
				data->columns = (TableNode::Data::ColumnInfo*)MemoryManager::pageAllocator.Allocate(sizeof(TableNode::Data::ColumnInfo) * data->numColumns);
			}

			if (!data->cells || !data->columns)
			{
				// TODO: allocation error
				return;
			}

			for (int n = 0; n < data->numRows * data->numColumns; n++)
			{
				data->cells[n] = nullptr;
			}

			// Fill a grid array with pointers to cells
			int rowIndex = 0;
			for (TableRowNode::Data* row = data->firstRow; row; row = row->nextRow)
			{
				int columnIndex = 0;

				for (TableCellNode::Data* cell = row->firstCell; cell; cell = cell->nextCell)
				{
					while (data->cells[rowIndex * data->numColumns + columnIndex] && columnIndex < data->numColumns)
					{
						columnIndex++;
					}
					if (columnIndex == data->numColumns)
					{
						break;
					}

					for (int j = 0; j < cell->rowSpan; j++)
					{
						for (int i = 0; i < cell->columnSpan; i++)
						{
							data->cells[(rowIndex + j) * data->numColumns + columnIndex + i] = cell;
						}
					}

					cell->columnIndex = columnIndex;
					cell->rowIndex = rowIndex;
					columnIndex += cell->columnSpan;
				}
				rowIndex++;
			}
		}

		for (int n = 0; n < data->numColumns; n++)
		{
			data->columns[n].preferredWidth = 16;
		}

		for (TableRowNode::Data* row = data->firstRow; row; row = row->nextRow)
		{
			for (TableCellNode::Data* cell = row->firstCell; cell; cell = cell->nextCell)
			{
				int preferredWidth = (2 * data->cellPadding + cell->node->size.x) / cell->columnSpan;

				for (int i = 0; i < cell->columnSpan; i++)
				{
					if (data->columns[cell->columnIndex + i].preferredWidth < preferredWidth)
					{
						data->columns[cell->columnIndex + i].preferredWidth = preferredWidth;
					}
				}
			}
		}

		int totalCellSpacing = (data->numColumns + 1) * data->cellSpacing;
		int totalPreferredWidth = 0;
		for (int i = 0; i < data->numColumns; i++)
		{
			totalPreferredWidth += data->columns[i].preferredWidth;
		}

		int maxAvailableWidthForCells = layout.MaxAvailableWidth() - totalCellSpacing;
		if (totalPreferredWidth > maxAvailableWidthForCells)
		{
			for (int i = 0; i < data->numColumns; i++)
			{
				data->columns[i].preferredWidth = ((long)maxAvailableWidthForCells * data->columns[i].preferredWidth) / totalPreferredWidth;
			}
		}

		int totalWidth = totalCellSpacing;
		for (int i = 0; i < data->numColumns; i++)
		{
			totalWidth += data->columns[i].preferredWidth;
		}
		data->totalWidth = totalWidth;
		node->size.x = totalWidth;

		layout.PushCursor();
		layout.PushLayout();

		int available = layout.AvailableWidth();
		if (totalWidth < available)
		{
			int alignmentPadding = 0;

			if (node->style.alignment == ElementAlignment::Center)
			{
				alignmentPadding = (available - totalWidth) / 2;
			}
			else if (node->style.alignment == ElementAlignment::Right)
			{
				alignmentPadding = (available - totalWidth);
			}
			if (alignmentPadding)
			{
				layout.PadHorizontal(alignmentPadding, 0);
				node->anchor = layout.Cursor();
			}
		}

		data->state = Data::FinalisingLayout;
		layout.RecalculateLayoutForNode(node);
		
		layout.PopLayout();
		layout.PopCursor();

		data->state = Data::FinishedLayout;
	}

	for (TableRowNode::Data* row = data->firstRow; row; row = row->nextRow)
	{
		if (!row->nextRow)
		{
			// Use last row's dimensions to calculate how tall the table should be
			int bottom = row->node->anchor.y + row->node->size.y;
			node->size.y = bottom - node->anchor.y + data->cellSpacing;
		}
	}

	layout.tableDepth--;
	layout.PadVertical(node->size.y);
	layout.BreakNewLine();
}

// Table row node

Node* TableRowNode::Construct(Allocator& allocator)
{
	TableRowNode::Data* data = allocator.Alloc<TableRowNode::Data>();
	if (data)
	{
		data->node = allocator.Alloc<Node>(Node::TableRow, data);
		return data->node;
	}
	return nullptr;
}

void TableRowNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableRowNode::Data* data = static_cast<TableRowNode::Data*>(node->data);

	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);
	if (tableData)
	{
		if (tableData->IsGeneratingLayout())
		{
			if (!tableData->HasGeneratedCellGrid())
			{
				if (!tableData->firstRow)
				{
					tableData->firstRow = data;
				}
				else
				{
					for (TableRowNode::Data* row = tableData->firstRow; row; row = row->nextRow)
					{
						if (row->nextRow == nullptr)
						{
							row->nextRow = data;
							break;
						}
					}
				}
				data->rowIndex = tableData->numRows;
				tableData->numRows++;
			}
		}
		else
		{
			node->anchor = layout.Cursor();
			node->size.x = tableData->totalWidth;
		}
	}

	layout.PushCursor();
	//layout.BreakNewLine();
	layout.PushLayout();
}

void TableRowNode::EndLayoutContext(Layout& layout, Node* node)
{
	TableRowNode::Data* data = static_cast<TableRowNode::Data*>(node->data);
	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);

	if (tableData)
	{
		layout.PopLayout();
		layout.BreakNewLine();
		layout.PopCursor();

		if (!tableData->IsGeneratingLayout())
		{
			node->size.y = 0;
			for (TableCellNode::Data* cellData = data->firstCell; cellData; cellData = cellData->nextCell)
			{
				int cellBottom = cellData->node->anchor.y + cellData->node->size.y;
				if (cellBottom > node->anchor.y + node->size.y)
				{
					node->size.y = cellBottom - node->anchor.y;
				}
			}
			for (TableCellNode::Data* cellData = data->firstCell; cellData; cellData = cellData->nextCell)
			{
				cellData->node->size.y = node->size.y;
			}
			layout.PadVertical(node->size.y + tableData->cellSpacing);
		}
	}
}

void TableRowNode::ApplyStyle(Node* node)
{
	node->style.alignment = ElementAlignment::Left;
}

// Table cell node

Node* TableCellNode::Construct(Allocator& allocator, bool isHeader)
{
	TableCellNode::Data* data = allocator.Alloc<TableCellNode::Data>(isHeader);
	if (data)
	{
		Node* node = allocator.Alloc<Node>(Node::TableCell, data);
		data->node = node;
		return node;
	}
	return nullptr;
}

void TableCellNode::ApplyStyle(Node* node)
{
	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);
	if (data->isHeader)
	{
		node->style.alignment = ElementAlignment::Center;
		node->style.fontStyle = FontStyle::Bold;
	}
	else
	{
		node->style.alignment = ElementAlignment::Left;
	}
}


void TableCellNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);
	TableRowNode::Data* rowData = node->FindParentDataOfType<TableRowNode::Data>(Node::TableRow);

	layout.PushLayout();

	if (tableData && rowData)
	{
		if (tableData->IsGeneratingLayout())
		{
			if (!tableData->HasGeneratedCellGrid())
			{
				if (!rowData->firstCell)
				{
					rowData->firstCell = data;
				}
				else
				{
					for (TableCellNode::Data* cell = rowData->firstCell; cell; cell = cell->nextCell)
					{
						if (!cell->nextCell)
						{
							cell->nextCell = data;
							break;
						}
					}
				}

				data->rowIndex = rowData->rowIndex;
				data->columnIndex = rowData->numCells;
				rowData->numCells++;
			}
		}
		else
		{
			node->anchor = layout.Cursor();

			node->size.x = tableData->columns[data->columnIndex].preferredWidth;
			for (int n = 1; n < data->columnSpan && n < tableData->numColumns; n++)
			{
				node->size.x += tableData->columns[data->columnIndex + n].preferredWidth + tableData->cellSpacing;
			}

			layout.RestrictHorizontal(node->size.x);
			layout.PadHorizontal(tableData->cellPadding, tableData->cellPadding);
		}
	}

	layout.PushCursor();

	if (tableData)
	{
		layout.PadVertical(tableData->cellPadding);
	}
}

void TableCellNode::EndLayoutContext(Layout& layout, Node* node)
{
	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);
	TableRowNode::Data* rowData = node->FindParentDataOfType<TableRowNode::Data>(Node::TableRow);

	if (tableData)
	{
		Rect rect;
		node->CalculateEncapsulatingRect(rect);
		node->size.y = rect.y + rect.height + tableData->cellPadding - node->anchor.y;

		if (tableData->IsGeneratingLayout())
		{
			node->size.x = rect.width;
		}
	}

	layout.BreakNewLine();
	layout.PopCursor();
	layout.PopLayout();

	if (tableData && !tableData->IsGeneratingLayout())
	{
		layout.PadHorizontal(node->size.x + tableData->cellSpacing, 0);
	}
}

void TableCellNode::Draw(DrawContext& context, Node* node)
{
	uint8_t borderColour = Platform::video->colourScheme.textColour;

	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);
	Node* rowNode = node->FindParentOfType(Node::TableRow);
	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	if (tableData && !tableData->IsGeneratingLayout() && rowNode)
	{
		int x = node->anchor.x;
		int y = node->anchor.y;
		int w = node->size.x;
		int h = node->size.y;

		bool needsFill = context.surface->bpp > 1
			&& data->bgColour != TRANSPARENT_COLOUR_VALUE
			&& data->bgColour != tableData->bgColour;

		if (tableData->border)
		{
			context.surface->HLine(context, x, y, w, borderColour);
			context.surface->HLine(context, x, y + h - 1, w, borderColour);
			context.surface->VLine(context, x, y + 1, h - 2, borderColour);
			context.surface->VLine(context, x + w - 1, y + 1, h - 2, borderColour);
			if (needsFill)
			{
				context.surface->FillRect(context, x + 1, y + 1, w - 2, h - 2, data->bgColour);
			}
		}
		else if (data->bgColour != TRANSPARENT_COLOUR_VALUE && context.surface->bpp > 1)
		{
			if (needsFill)
			{
				context.surface->FillRect(context, x, y, w, h, data->bgColour);
			}
		}
	}

}
