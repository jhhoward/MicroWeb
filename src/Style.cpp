#include <string.h>
#include "Style.h"
#include "Memory/Memory.h"

static StylePool pool;

StylePool& StylePool::Get()
{
	return pool;
}

ElementStyleHandle StylePool::AddStyle(const ElementStyle& style)
{
	// Check if an identical style already exists
	for (ElementStyleHandle n = 0; n < numItems; n++)
	{
		if (!memcmp(&style, &GetStyle(n), sizeof(ElementStyle)))
		{
			return n;
		}
	}

	ElementStyleHandle newHandle = numItems;
	if (newHandle >= MAX_STYLES)
		return 0;

	int chunkIndex = newHandle >> STYLE_POOL_CHUNK_SHIFT;
	int itemIndex = newHandle & STYLE_POOL_INDEX_MASK;

	if (chunkIndex > 0 && itemIndex == 0)
	{
		chunks[chunkIndex] = MemoryManager::pageAllocator.Alloc<StylePool::PoolChunk>();
		if (!chunks[chunkIndex])
		{
			return 0;
		}
	}

	chunks[chunkIndex]->items[itemIndex] = style;
	numItems++;

	return newHandle;
}

const ElementStyle& StylePool::GetStyle(ElementStyleHandle handle)
{
	if (handle < numItems)
	{
		int chunkIndex = handle >> STYLE_POOL_CHUNK_SHIFT;
		return chunks[chunkIndex]->items[handle & STYLE_POOL_INDEX_MASK];
	}
	return chunks[0]->items[0];
}

void StylePool::Init()
{
	chunks[0] = new StylePool::PoolChunk();
}
