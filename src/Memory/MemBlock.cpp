#include <string.h>
#include "MemBlock.h"
#include "LinAlloc.h"
#include "Memory.h"
#include "../Platform.h"
#include "../App.h"

#ifdef __DOS__
#include <dos.h>
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
	if (App::config.useSwap)
	{
		swapFile = fopen("Microweb.swp", "wb+");
	}

	if (swapFile)
	{
		swapBuffer = malloc(MAX_SWAP_ALLOCATION);
		lastSwapRead = -1;
		swapFileLength = 0;
		maxSwapSize = MAX_SWAP_SIZE;
	}

#ifdef __DOS__
	if (App::config.useEMS)
	{
		ems.Init();
	}
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
	long conventionalMemoryAvailable = 0;

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

	conventionalMemoryAvailable += _memmax();
#endif
	
	conventionalMemoryAvailable += MemoryManager::pageAllocator.TotalAllocated() - MemoryManager::pageAllocator.TotalUsed();

	if (swapFile && conventionalMemoryAvailable < 16 * 1024)	// If we have less than 16K available, fall back to disk
	{
		uint16_t sizeNeededForSwap = size + sizeof(uint16_t);

		if (sizeNeededForSwap <= MAX_SWAP_ALLOCATION && swapFileLength + sizeNeededForSwap + sizeof(uint16_t) < maxSwapSize)
		{
			result.swapFilePosition = swapFileLength;
			fseek(swapFile, swapFileLength, SEEK_SET);

			fwrite(&size, sizeof(uint16_t), 1, swapFile);

			char empty = 0xaa;
			size_t bytesLeft = size;
			while (bytesLeft > 0)
			{
				if(!fwrite(&empty, 1, 1, swapFile))
				{
					// Out of disk space?
					result.type = MemBlockHandle::Unallocated;
					return result;
				}
				bytesLeft--;
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
