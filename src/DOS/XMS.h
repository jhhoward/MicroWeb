#ifndef _XMS_H_
#define _XMS_H_

#include <stdint.h>
#include "../Memory/MemBlock.h"

#define XMS_BUFFER_SIZE 4096
#define MAX_XMS_ALLOCATION_KB (16 * 1024)		// Max out at 16MB because we are storing 24-bit pointers

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
