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

	uint8_t borderColour = Platform::video->colourScheme.textColour;
	int x = node->anchor.x - data->cellSpacing - data->cellPadding;
	int y = node->anchor.y - data->cellSpacing - data->cellPadding;
	int w = node->size.x + (data->cellSpacing + data->cellPadding) * 2;
	int h = node->size.y + (data->cellSpacing + data->cellPadding) * 2;
	context.surface->HLine(context, x, y, w, borderColour);
	context.surface->HLine(context, x, y + h - 1, w, borderColour);
	context.surface->VLine(context, x, y + 1, h - 2, borderColour);
	context.surface->VLine(context, x + w - 1, y + 1, h - 2, borderColour);
}

void TableNode::BeginLayoutContext(Layout& layout, Node* node)
{
	TableNode::Data* data = static_cast<TableNode::Data*>(node->data);

	if (data->state == Data::FinishedLayout)
	{
		data->state = Data::GeneratingLayout;
	}

	layout.BreakNewLine();
	layout.PushCursor();
	layout.PushLayout();

	layout.PadHorizontal(data->cellSpacing, data->cellSpacing);
	
	if (!data->IsGeneratingLayout())
	{
		if (data->numRows > 0)
		{
			layout.PadVertical(data->cellSpacing + data->cellPadding);
		}
	}
}

void TableNode::EndLayoutContext(Layout& layout, Node* node)
{
	//NodeHandler::EndLayoutContext(layout, node);
	node->EncapsulateChildren();

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
				data->cells = (TableCellNode::Data**)MemoryManager::pageAllocator.Alloc(sizeof(TableCellNode::Data*) * data->numRows * data->numColumns);
			}

			if (!data->columns)
			{
				data->columns = (TableNode::Data::ColumnInfo*)MemoryManager::pageAllocator.Alloc(sizeof(TableNode::Data::ColumnInfo) * data->numColumns);
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
							data->cells[j * data->numColumns + i] = cell;
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

		int totalPreferredWidth = data->cellSpacing * (data->numColumns - 1);
		for (int i = 0; i < data->numColumns; i++)
		{
			totalPreferredWidth += data->columns[i].preferredWidth;
		}

		int maxAvailableWidth = layout.MaxAvailableWidth() - data->cellSpacing * 2;
		if (totalPreferredWidth > maxAvailableWidth)
		{
			// TODO resize widths to fit
			for (int i = 0; i < data->numColumns; i++)
			{
				data->columns[i].preferredWidth = ((long)maxAvailableWidth * data->columns[i].preferredWidth) / totalPreferredWidth;
			}
		}

		data->state = Data::FinalisingLayout;
		layout.RecalculateLayoutForNode(node);
		layout.PadVertical(node->size.y + (data->cellPadding + data->cellSpacing) * 2);
		data->state = Data::FinishedLayout;
	}

	layout.BreakNewLine();
}

// Table row node

Node* TableRowNode::Construct(Allocator& allocator)
{
	TableRowNode::Data* data = allocator.Alloc<TableRowNode::Data>();
	if (data)
	{
		return allocator.Alloc<Node>(Node::TableRow, data);
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
			layout.PadVertical(tableData->cellPadding);
		}
	}

	layout.PushCursor();
	//layout.BreakNewLine();
	layout.PushLayout();
}

void TableRowNode::EndLayoutContext(Layout& layout, Node* node)
{
	//NodeHandler::EndLayoutContext(layout, node);
	node->EncapsulateChildren();

	TableRowNode::Data* data = static_cast<TableRowNode::Data*>(node->data);
	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);

	if (tableData)
	{
		layout.PopLayout();
		layout.BreakNewLine();
		layout.PopCursor();
		layout.PadVertical(node->size.y + tableData->cellSpacing + tableData->cellPadding);
	}
}

// Table cell node

Node* TableCellNode::Construct(Allocator& allocator)
{
	TableCellNode::Data* data = allocator.Alloc<TableCellNode::Data>();
	if (data)
	{
		Node* node = allocator.Alloc<Node>(Node::TableCell, data);
		data->node = node;
		return node;
	}
	return nullptr;
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
			layout.RestrictHorizontal(tableData->columns[data->columnIndex].preferredWidth);
			layout.PadHorizontal(tableData->cellPadding, tableData->cellPadding);
		}
	}

	layout.PushCursor();
}

void TableCellNode::EndLayoutContext(Layout& layout, Node* node)
{
	//NodeHandler::EndLayoutContext(layout, node);
	if (node->firstChild)
	{
		node->EncapsulateChildren();
	}
	else
	{
		// Cell was empty
		node->anchor = layout.GetCursor();
	}

	layout.BreakNewLine();
	layout.PopCursor();
	layout.PopLayout();

	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);
	TableRowNode::Data* rowData = node->FindParentDataOfType<TableRowNode::Data>(Node::TableRow);
	if (tableData && rowData)
	{
		if (tableData->IsGeneratingLayout())
		{
		}
		else
		{
			layout.PadHorizontal(tableData->columns[data->columnIndex].preferredWidth + tableData->cellSpacing, 0);
		}
	}

//	layout.PadHorizontal(200, 0);
}

void TableCellNode::Draw(DrawContext& context, Node* node)
{
	uint8_t borderColour = Platform::video->colourScheme.textColour;

	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);
	Node* rowNode = node->FindParentOfType(Node::TableRow);
	TableCellNode::Data* data = static_cast<TableCellNode::Data*>(node->data);

	if (tableData && !tableData->IsGeneratingLayout() && rowNode)
	{
		int x = node->anchor.x - tableData->cellPadding;
		int y = node->anchor.y - tableData->cellPadding;
		int w = tableData->columns[data->columnIndex].preferredWidth;
		int h = rowNode->size.y + 2 * tableData->cellPadding;

		context.surface->HLine(context, x, y, w, borderColour);
		context.surface->HLine(context, x, y + h - 1, w, borderColour);
		context.surface->VLine(context, x, y + 1, h - 2, borderColour);
		context.surface->VLine(context, x + w - 1, y + 1, h - 2, borderColour);
	}

}
