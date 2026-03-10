#ifndef _XMS_H_
#define _XMS_H_

#include <stdint.h>
#include "../Memory/MemBlock.h"

#define XMS_BUFFER_SIZE 1024

class XMSManager
{
public:
	XMSManager() : isAvailable(false) {}

	void Init();
	void Reset();

	bool IsAvailable() { return isAvailable; }

	MemBlockHandle Allocate(size_t size);
	void* MapBlock(MemBlockHandle& handle);
	void Commit(MemBlockHandle& handle);

	void Shutdown();

	long TotalAllocated() { return totalAllocated; }
	long TotalUsed() { return totalUsed; }

private:
	bool isAvailable;

	long totalAllocated;
	long totalUsed;

	void* buffer;
	uint16_t allocationHandle;
};

#endif
