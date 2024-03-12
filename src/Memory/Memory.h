#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "Alloc.h"
#include "LinAlloc.h"
#include "MemBlock.h"

class MemoryManager
{
public:
	static LinearAllocator pageAllocator;
	static MallocWrapper interfaceAllocator;

	static MemBlockAllocator pageBlockAllocator;

	static void GenerateMemoryReport(char* outString);
};


#endif
