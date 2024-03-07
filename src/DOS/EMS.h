#ifndef _EMS_H_
#define _EMS_H_

#include <stdint.h>
#include "../Memory/MemBlock.h"

#define EMS_PAGE_SIZE (16 * 1024l)

class EMSManager
{
public:
	EMSManager() : isAvailable(false) {}

	void Init();
	void Reset();

	bool IsAvailable() { return isAvailable; }

	MemBlockHandle Allocate(size_t size);
	void* MapBlock(MemBlockHandle& handle);

	void Shutdown();

private:
	bool isAvailable;
	int numAllocatedPages;
	uint16_t pageAddressSegment;
	uint16_t allocationHandle;

	uint16_t allocationPageIndex;
	uint16_t allocationPageUsed;
	uint16_t mappedPage;
};

#endif
