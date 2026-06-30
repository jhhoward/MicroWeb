#include "LinAlloc.h"

LinearAllocator::LinearAllocator() : allocOffset(0), numAllocatedChunks(1), totalBytesUsed(0), errorFlag(Error_None)
{
	currentChunk = firstChunk = new Chunk();
}

LinearAllocator::~LinearAllocator()
{
	for (Chunk* ptr = firstChunk; ptr;)
	{
		Chunk* next = ptr->next;
		delete ptr;
		ptr = next;
	}
}

void LinearAllocator::Reset()
{
	currentChunk = firstChunk;
	allocOffset = 0;
	totalBytesUsed = 0;
	errorFlag = Error_None;
}

void* LinearAllocator::Allocate(size_t numBytes)
{
	// Force 16-bit alignment by always allocating an even number of bytes
	numBytes += numBytes & (LINEAR_ALLOC_ALIGNMENT - 1);

	if (numBytes >= CHUNK_DATA_SIZE)
	{
		errorFlag = Error_AllocationTooLarge;
		return NULL;
	}

	if (!currentChunk)
	{
		errorFlag = Error_OutOfMemory;
		return nullptr;
	}

	uint8_t* result = &currentChunk->data[allocOffset];

	if (allocOffset + numBytes > CHUNK_DATA_SIZE)
	{
		// Need to allocate from the next chunk

		if (!currentChunk->next)
		{
			currentChunk->next = new Chunk();

			if (!currentChunk->next)
			{
				errorFlag = Error_OutOfMemory;
				return NULL;
			}
			numAllocatedChunks++;
		}

		currentChunk = currentChunk->next;
		allocOffset = 0;
		result = &currentChunk->data[allocOffset];
	}

	totalBytesUsed += (long)numBytes;
	allocOffset += numBytes;
	return NormalizeFarPointer(result);
}
