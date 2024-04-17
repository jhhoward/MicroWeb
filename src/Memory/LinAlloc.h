//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef _LINALLOC_H_
#define _LINALLOC_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include "Alloc.h"
#pragma warning(disable:4996)

// 16K chunk size including next chunk pointer
#define CHUNK_DATA_SIZE (16 * 1024 - sizeof(struct Chunk*))

class LinearAllocator : public Allocator
{
	struct Chunk
	{
		Chunk() : next(NULL) {}
		uint8_t data[CHUNK_DATA_SIZE];
		Chunk* next;
	};

public:
	enum AllocationError
	{
		Error_None,
		Error_AllocationTooLarge,
		Error_OutOfMemory
	};

	LinearAllocator() : allocOffset(0), numAllocatedChunks(1), totalBytesUsed(0), errorFlag(Error_None)
	{
		currentChunk = firstChunk = new Chunk();
	}

	~LinearAllocator()
	{
		for (Chunk* ptr = firstChunk; ptr;)
		{
			Chunk* next = ptr->next;
			delete ptr;
			ptr = next;
		}
	}

	void Reset()
	{
		currentChunk = firstChunk;
		allocOffset = 0;
		totalBytesUsed = 0;
		errorFlag = Error_None;
	}

	virtual void* Allocate(size_t numBytes)
	{
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

		totalBytesUsed += (long) numBytes;
		allocOffset += numBytes;
		return result;
	}

	long TotalAllocated() { return numAllocatedChunks * sizeof(Chunk); }
	long TotalUsed() { return totalBytesUsed; }
	AllocationError GetError() { return errorFlag; }

private:

	Chunk* firstChunk;
	Chunk* currentChunk;
	size_t allocOffset;

	long numAllocatedChunks;
	long totalBytesUsed;		// Bytes actually used for data
	AllocationError errorFlag;
};

#endif
