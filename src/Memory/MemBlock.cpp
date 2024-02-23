#include "MemBlock.h"
#include "LinAlloc.h"
#include "Memory.h"

void* MemBlockHandle::GetPtr()
{
	switch (type)
	{
	case MemBlockHandle::Conventional:
		return conventionalPointer;
	case MemBlockHandle::DiskSwap:
	{
		return MemoryManager::pageBlockAllocator.AccessSwap(*this);
	}
	default:
		return nullptr;
	}
}

void MemBlockHandle::Commit()
{
	switch (type)
	{
	case MemBlockHandle::DiskSwap:
		MemoryManager::pageBlockAllocator.CommitSwap(*this);
		break;
	
	}
}

MemBlockAllocator::MemBlockAllocator()
{
	//swapFile = fopen("Microweb.swp", "w+");
	swapFile = NULL;
	if (swapFile)
	{
		swapBuffer = malloc(MAX_SWAP_ALLOCATION);
		lastSwapRead = -1;
		swapFileLength = 0;
	}
}


MemBlockHandle MemBlockAllocator::Allocate(size_t size)
{
	MemBlockHandle result;

	if (swapFile && size <= MAX_SWAP_ALLOCATION)
	{
		result.swapFilePosition = swapFileLength;
		result.allocatedSize = size;
		fseek(swapFile, swapFileLength, SEEK_SET);

		char empty[32];
		size_t toWrite = size;
		while (toWrite > 0)
		{
			if (toWrite >= 32)
			{
				fwrite(empty, 1, 32, swapFile);
				toWrite -= 32;
			}
			else
			{
				fwrite(empty, 1, toWrite, swapFile);
				break;
			}
		}
		swapFileLength += size;
		result.type = MemBlockHandle::DiskSwap;
	}
	else
	{
		result.conventionalPointer = MemoryManager::pageAllocator.Alloc(size);
		if (result.conventionalPointer)
		{
			result.type = MemBlockHandle::Conventional;
		}
	}

	return result;
}

void* MemBlockAllocator::AccessSwap(MemBlockHandle& handle)
{
	if (lastSwapRead != handle.swapFilePosition)
	{
		fseek(swapFile, handle.swapFilePosition, SEEK_SET);
		fread(swapBuffer, 1, handle.allocatedSize, swapFile);
		lastSwapRead = handle.swapFilePosition;
	}

	return swapBuffer;
}

void MemBlockAllocator::CommitSwap(MemBlockHandle& handle)
{
	fseek(swapFile, handle.swapFilePosition, SEEK_SET);
	fwrite(swapBuffer, 1, handle.allocatedSize, swapFile);
}
