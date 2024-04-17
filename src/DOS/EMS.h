#ifndef _EMS_H_
#define _EMS_H_

#include <stdint.h>
#include "../Memory/MemBlock.h"

#define EMS_PAGE_SIZE (16 * 1024l)
#define NUM_MAPPABLE_PAGES 4
#define EMS_PAGE_SEGMENT_SPACING (1024)

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

	long TotalAllocated() { return numAllocatedPages * EMS_PAGE_SIZE; }
	long TotalUsed() { return allocationPageIndex * EMS_PAGE_SIZE + allocationPageUsed; }

private:
	bool isAvailable;
	int numAllocatedPages;
	uint16_t pageAddressSegment;
	uint16_t allocationHandle;

	uint16_t allocationPageIndex;
	uint16_t allocationPageUsed;

	uint16_t mappedPages[NUM_MAPPABLE_PAGES];
	uint8_t nextPageToMap;
};

#endif
