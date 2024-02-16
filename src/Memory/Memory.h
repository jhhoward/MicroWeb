#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "LinAlloc.h"
#include "MemBlock.h"

class MemoryManager
{
public:
	static LinearAllocator pageAllocator;
	static LinearAllocator interfaceAllocator;

	static MemBlockAllocator pageBlockAllocator;
};


#endif
