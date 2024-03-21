#include <string.h>
#include "MemBlock.h"
#include "LinAlloc.h"
#include "Memory.h"
#include "../Platform.h"

#ifdef __DOS__
#include "../DOS/EMS.h"

EMSManager ems;
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
		
		Platform::FatalError("Invalid pointer type: %d\n", type);
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
	//swapFile = fopen("Microweb.swp", "wb+");
	
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
		strcpy(result.Get<char*>(), inString);
		result.Commit();
	}
	return result;
}

MemBlockHandle MemBlockAllocator::Allocate(uint16_t size)
{
	MemBlockHandle result;

#ifdef __DOS__
	if (ems.IsAvailable())
	{
		result = ems.Allocate(size);
		if (result.IsAllocated())
		{
			totalAllocated += size;
			return result;
		}
	}
#endif

	if (swapFile)
	{
		uint16_t sizeNeededForSwap = size + sizeof(uint16_t);

		if (sizeNeededForSwap <= MAX_SWAP_ALLOCATION && swapFileLength + sizeNeededForSwap + sizeof(uint16_t) < maxSwapSize)
		{
			result.swapFilePosition = swapFileLength;
			fseek(swapFile, swapFileLength, SEEK_SET);

			fwrite(&size, sizeof(uint16_t), 1, swapFile);

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

			swapFileLength += sizeNeededForSwap;
			result.type = MemBlockHandle::DiskSwap;
			totalAllocated += sizeNeededForSwap;
			return result;
		}
	}

	//if(0)
	{
		result.conventionalPointer = MemoryManager::pageAllocator.Allocate(size);
		if (result.conventionalPointer)
		{
			result.type = MemBlockHandle::Conventional;
			totalAllocated += size;
		}
	}

	return result;
}

void* MemBlockAllocator::AccessSwap(MemBlockHandle& handle)
{
	if (swapFile && lastSwapRead != handle.swapFilePosition)
	{
		fseek(swapFile, handle.swapFilePosition, SEEK_SET);
		uint16_t allocatedSize = 0;
		fread(&allocatedSize, sizeof(uint16_t), 1, swapFile);
		fread(swapBuffer, 1, allocatedSize, swapFile);
		lastSwapRead = handle.swapFilePosition;
	}

	return swapBuffer;
}

void MemBlockAllocator::CommitSwap(MemBlockHandle& handle)
{
	if (swapFile)
	{
		fseek(swapFile, handle.swapFilePosition, SEEK_SET);
		uint16_t allocatedSize = 0;
		fread(&allocatedSize, sizeof(uint16_t), 1, swapFile);
		fseek(swapFile, handle.swapFilePosition + sizeof(uint16_t), SEEK_SET);
		fwrite(swapBuffer, 1, allocatedSize, swapFile);
	}
}

void MemBlockAllocator::Reset()
{
	swapFileLength = 0;
	lastSwapRead = -1;
	totalAllocated = 0;

#ifdef __DOS__
	ems.Reset();
#endif
}
