#include <stdio.h>
#include "../Layout.h"
#include "../Memory/Memory.h"
#include "../Draw/Surface.h"
#include "../VidModes.h"
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
	bool needsFill = data->bgColour != TRANSPARENT_COLOUR_VALUE && context.surface->format != DrawSurface::Format_1BPP;

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

		int minCellWidth = 16;

		for (int n = 0; n < data->numColumns; n++)
		{
			data->columns[n].Clear();
			//data->columns[n].preferredWidth = minCellWidth;
		}

		// Cull unused columns
		for (int n = data->numColumns - 1; n >= 0; n--)
		{
			bool isUsed = false;

			for (TableRowNode::Data* row = data->firstRow; row && !isUsed; row = row->nextRow)
			{
				for (TableCellNode::Data* cell = row->firstCell; cell && !isUsed; cell = cell->nextCell)
				{
					if (cell->columnIndex == n)
					{
						isUsed = true;
					}
				}
			}

			if (isUsed)
			{
				break;
			}
			else
			{
				data->numColumns--;
			}
		}

		int maxConstrainedTableWidth = layout.MaxAvailableWidth();
		if (data->explicitWidth.IsSet())
		{
			maxConstrainedTableWidth = layout.CalculateWidth(data->explicitWidth);
		}

		// Find out preferred column widths in two passes:
		// - First pass, check with cells of column span = 1
		// - Second pass for cells of column span > 1
		for (int pass = 0; pass < 2; pass++)
		{
			for (TableRowNode::Data* row = data->firstRow; row; row = row->nextRow)
			{
				for (TableCellNode::Data* cell = row->firstCell; cell; cell = cell->nextCell)
				{
					if (pass == 0 && cell->columnSpan > 1)
						continue;
					if (pass == 1 && cell->columnSpan == 1)
						continue;

					int preferredWidth = (2 * data->cellPadding + cell->node->size.x);
					int explicitWidth = 0;
					int explicitWidthPercentage = 0;

					if (cell->explicitWidth.IsSet())
					{
						if (cell->explicitWidth.IsPercentage())
						{
							explicitWidthPercentage = cell->explicitWidth.Value();
						}
						else
						{
							explicitWidth = ((long)cell->explicitWidth.Value() * Platform::video->GetVideoModeInfo()->zoom) / 100;
						}
					}

					if (preferredWidth > maxConstrainedTableWidth)
					{
						preferredWidth = maxConstrainedTableWidth;
					}
					if (explicitWidth > maxConstrainedTableWidth)
					{
						explicitWidth = maxConstrainedTableWidth;
					}
					
					if (pass == 0)
					{
						if (data->columns[cell->columnIndex].preferredWidth < preferredWidth)
						{
							data->columns[cell->columnIndex].preferredWidth = preferredWidth;
						}
						if (data->columns[cell->columnIndex].explicitWidthPercentage < explicitWidthPercentage)
						{
							data->columns[cell->columnIndex].explicitWidthPercentage = explicitWidthPercentage;
						}
						if (data->columns[cell->columnIndex].explicitWidthPixels < explicitWidth)
						{
							data->columns[cell->columnIndex].explicitWidthPixels = explicitWidth;
						}
					}
					else
					{
						int columnsPreferredWidth = data->cellSpacing * (cell->columnSpan - 1);
						int columnsExplicitWidthPercentage = 0;
						int columnsExplicitWidthPixels = 0;

						for (int i = 0; i < cell->columnSpan; i++)
						{
							columnsPreferredWidth += data->columns[cell->columnIndex + i].preferredWidth;
							columnsExplicitWidthPercentage += data->columns[cell->columnIndex + i].explicitWidthPercentage;
							columnsExplicitWidthPixels += data->columns[cell->columnIndex + i].explicitWidthPixels;
						}

						if (columnsPreferredWidth < preferredWidth)
						{
							for (int i = 0; i < cell->columnSpan; i++)
							{
								data->columns[cell->columnIndex + i].preferredWidth += (preferredWidth - columnsPreferredWidth) / cell->columnSpan;
							}
						}
						if (columnsExplicitWidthPercentage < explicitWidthPercentage)
						{
							for (int i = 0; i < cell->columnSpan; i++)
							{
								data->columns[cell->columnIndex + i].explicitWidthPercentage += (explicitWidthPercentage - columnsExplicitWidthPercentage) / cell->columnSpan;
							}
						}
						if (columnsExplicitWidthPixels < explicitWidth)
						{
							for (int i = 0; i < cell->columnSpan; i++)
							{
								data->columns[cell->columnIndex + i].explicitWidthPixels += (explicitWidth - columnsExplicitWidthPixels) / cell->columnSpan;
							}
						}
					}
				}
			}
		}

		data->totalWidth = 0;
		int totalCellSpacing = (data->numColumns + 1) * data->cellSpacing;

		if (!data->explicitWidth.IsSet())
		{
			// Need to calculate the width of the table as it wasn't specified
			int totalPreferredWidth = 0;
			int maxAvailableWidthForCells = layout.MaxAvailableWidth() - totalCellSpacing;
			int widthRemaining = maxAvailableWidthForCells;

			for (int i = 0; i < data->numColumns; i++)
			{
				if (data->columns[i].explicitWidthPixels)
				{
					data->columns[i].calculatedWidth = data->columns[i].explicitWidthPixels;
				}
				else if (data->columns[i].explicitWidthPercentage)
				{
					data->columns[i].calculatedWidth = 0;
				}
				else
				{
					data->columns[i].calculatedWidth = data->columns[i].preferredWidth;
				}
				totalPreferredWidth += data->columns[i].calculatedWidth;
			}

			// Enforce percentage constraints
			for (int it = 0; it < data->numColumns; it++)
			{
				bool changesMade = false;

				for (int i = 0; i < data->numColumns; i++)
				{
					if (data->columns[i].explicitWidthPercentage)
					{
						int desiredWidth = (data->columns[i].explicitWidthPercentage * totalPreferredWidth) / 100;

						if (desiredWidth != data->columns[i].calculatedWidth)
						{
							totalPreferredWidth -= data->columns[i].calculatedWidth;
							long z = data->columns[i].explicitWidthPercentage;
							data->columns[i].calculatedWidth = (z * totalPreferredWidth) / (100 - z);
							totalPreferredWidth += data->columns[i].calculatedWidth;
							changesMade = true;
						}
					}
				}

				if (!changesMade)
					break;
			}

			if (totalPreferredWidth <= maxAvailableWidthForCells)
			{
				data->totalWidth = totalPreferredWidth + totalCellSpacing;
				node->size.x = data->totalWidth;
			}
		}

		if (data->explicitWidth.IsSet() || !data->totalWidth)
		{
			// Generate widths for columns based on a given table width
			if (data->explicitWidth.IsSet())
			{
				data->totalWidth = layout.CalculateWidth(data->explicitWidth);
			}
			else
			{
				data->totalWidth = layout.MaxAvailableWidth();
			}
			node->size.x = data->totalWidth;

			int maxAvailableWidthForCells = node->size.x - totalCellSpacing;
			int widthRemaining = maxAvailableWidthForCells;
			int totalUnsetWidth = 0;
			int minUnsetWidth = 0;
			minCellWidth = data->numColumns ? data->totalWidth / (data->numColumns * 2) : 0;

			// First pass allocate widths to explicit pixels widths
			for (int i = 0; i < data->numColumns; i++)
			{
				if (data->columns[i].explicitWidthPixels)
				{
					data->columns[i].calculatedWidth = data->columns[i].explicitWidthPixels;
				}
				if (data->columns[i].explicitWidthPercentage)
				{
					int calculatedWidth = (long)(data->columns[i].explicitWidthPercentage * maxAvailableWidthForCells) / 100;
					if (calculatedWidth > data->columns[i].calculatedWidth)
					{
						data->columns[i].calculatedWidth = calculatedWidth;
					}
				}

				if (data->columns[i].calculatedWidth)
				{
					widthRemaining -= data->columns[i].calculatedWidth;
				}
				else
				{
					totalUnsetWidth += data->columns[i].preferredWidth;

					if(data->columns[i].preferredWidth)
						minUnsetWidth += minCellWidth;
				}
			}

			int totalCellsWidth = 0;

			if (widthRemaining < minUnsetWidth)
			{
				int widthForSetCells = maxAvailableWidthForCells - minUnsetWidth;
				int totalSetWidth = maxAvailableWidthForCells - widthRemaining;

				// Explicit cell widths too large to fit in table, readjust
				for (int i = 0; i < data->numColumns; i++)
				{
					if (!data->columns[i].calculatedWidth)
					{
						if (data->columns[i].preferredWidth)
							data->columns[i].calculatedWidth = minCellWidth;
					}
					else if(RESCALE_TO_FIT_SCREEN_WIDTH)
					{
						data->columns[i].calculatedWidth = ((long)widthForSetCells * data->columns[i].calculatedWidth) / totalSetWidth;
					}
					totalCellsWidth += data->columns[i].calculatedWidth;
				}
			}
			else
			{
				for (int i = 0; i < data->numColumns; i++)
				{
					if (!data->columns[i].calculatedWidth && totalUnsetWidth)
					{
						data->columns[i].calculatedWidth = ((long)widthRemaining * data->columns[i].preferredWidth) / totalUnsetWidth;
					}
					totalCellsWidth += data->columns[i].calculatedWidth;
				}
			}

			if (totalCellsWidth < maxAvailableWidthForCells && data->numColumns > 0)
			{
				data->columns[data->numColumns - 1].calculatedWidth += maxAvailableWidthForCells - totalCellsWidth;
			}
		}

		layout.PushCursor();
		layout.PushLayout();

		int available = layout.AvailableWidth();
		if (data->totalWidth < available)
		{
			int alignmentPadding = 0;

			if (node->GetStyle().alignment == ElementAlignment::Center)
			{
				alignmentPadding = (available - data->totalWidth) / 2;
			}
			else if (node->GetStyle().alignment == ElementAlignment::Right)
			{
				alignmentPadding = (available - data->totalWidth);
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

	layout.BreakNewLine();

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
	layout.PushLayout();
}

void TableRowNode::EndLayoutContext(Layout& layout, Node* node)
{
	TableRowNode::Data* data = static_cast<TableRowNode::Data*>(node->data);
	TableNode::Data* tableData = node->FindParentDataOfType<TableNode::Data>(Node::Table);

	if (tableData)
	{
		layout.BreakNewLine();
		layout.PopLayout();
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
	ElementStyle style = node->GetStyle();
	style.alignment = ElementAlignment::Left;
	node->SetStyle(style);
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
	ElementStyle style = node->GetStyle();

	if (data->isHeader)
	{
		style.alignment = ElementAlignment::Center;
		style.fontStyle = FontStyle::Bold;
	}
	else
	{
		style.alignment = ElementAlignment::Left;
	}
	node->SetStyle(style);
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

			node->size.x = tableData->columns[data->columnIndex].calculatedWidth;
			for (int n = 1; n < data->columnSpan && n < tableData->numColumns; n++)
			{
				node->size.x += tableData->columns[data->columnIndex + n].calculatedWidth + tableData->cellSpacing;
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

		bool needsFill = context.surface->format != DrawSurface::Format_1BPP
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
		else if (data->bgColour != TRANSPARENT_COLOUR_VALUE && context.surface->format != DrawSurface::Format_1BPP)
		{
			if (needsFill)
			{
				context.surface->FillRect(context, x, y, w, h, data->bgColour);
			}
		}
	}

}
