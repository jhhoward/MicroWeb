#include <string.h>
#include "MemBlock.h"
#include "LinAlloc.h"
#include "Memory.h"

#ifdef __DOS__
#include "../DOS/EMS.h"

static EMSManager ems;
#endif

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
#ifdef __DOS__
	case MemBlockHandle::EMS:
	{
		return ems.MapBlock(*this);
	}
#endif
	default:
		exit(1);
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
	: swapFile(nullptr)
	, swapFileLength(0)
	, swapBuffer(nullptr)
	, lastSwapRead(-1)
	, maxSwapSize(0)
{
}

void MemBlockAllocator::Init()
{
	// Disable swap for now
	//swapFile = fopen("Microweb.swp", "w+");
	
	if (swapFile)
	{
		swapBuffer = malloc(MAX_SWAP_ALLOCATION);
		lastSwapRead = -1;
		swapFileLength = 0;
		maxSwapSize = MAX_SWAP_SIZE;
	}

#ifdef __DOS__
	ems.Init();
#endif
}

void MemBlockAllocator::Shutdown()
{
#ifdef __DOS__
	ems.Shutdown();
#endif

	if (swapFile)
	{
		fclose(swapFile);
		swapFile = NULL;
	}
}

MemBlockHandle MemBlockAllocator::AllocString(const char* inString)
{
	MemBlockHandle result = Allocate(strlen(inString) + 1);
	if (result.IsAllocated())
	{
		strcpy(result.Get<char>(), inString);
		result.Commit();
	}
	return result;
}

MemBlockHandle MemBlockAllocator::Allocate(size_t size)
{
	MemBlockHandle result;

#ifdef __DOS__
	if (ems.IsAvailable())
	{
		result = ems.Allocate(size);
		if (result.IsAllocated())
		{
			return result;
		}
	}
#endif

	if (swapFile && size <= MAX_SWAP_ALLOCATION && swapFileLength + size < maxSwapSize)
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

void MemBlockAllocator::Reset()
{
	swapFileLength = 0;
	lastSwapRead = -1;

#ifdef __DOS__
	ems.Reset();
#endif
}
