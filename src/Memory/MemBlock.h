#ifndef _MEMBLOCK_H_
#define _MEMBLOCK_H_

#include <stdint.h>
#include <stdio.h>

#define MAX_SWAP_ALLOCATION (1024)
#define MAX_SWAP_SIZE (1024l * 1024l)

// Abstract way of allocating a chunk of memory from conventional memory, EMS, disk swap

struct MemBlockHandle
{
	enum Type
	{
		Unallocated,
		Conventional,
		EMS,
		DiskSwap
	};

	Type type;

	MemBlockHandle() : type(Unallocated), allocatedSize(0) {}
	MemBlockHandle(void* buffer) : type(Conventional), conventionalPointer(buffer) {}
	void* GetPtr();
	
	template <typename T>
	inline T* Get() { return (T*)GetPtr(); }
	void Commit();

	bool IsAllocated() { return type != Unallocated; }

	union
	{
		void* conventionalPointer;
		long swapFilePosition;

		struct
		{
			uint16_t emsPage;
			uint16_t emsPageOffset;
		};
	};
	size_t allocatedSize;
};

class LinearAllocator;

class MemBlockAllocator
{
public:
	MemBlockAllocator();

	void Init();
	void Shutdown();

	MemBlockHandle Allocate(size_t size);
	MemBlockHandle AllocString(const char* inString);

	void Reset();

private:
	friend struct MemBlockHandle;
	void* AccessSwap(MemBlockHandle& handle);
	void CommitSwap(MemBlockHandle& handle);

	FILE* swapFile;
	long swapFileLength;
	void* swapBuffer;
	long lastSwapRead;
	long maxSwapSize;
};


#endif
