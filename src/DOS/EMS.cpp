#include "EMS.h"
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EMS_INTERRUPT_NUMBER 0x67

void EMSManager::Init()
{
    union REGS inregs, outregs;
    struct SREGS sregs;

    // Check for EMS driver
    inregs.h.ah = 0x35;
    inregs.h.al = EMS_INTERRUPT_NUMBER;
    int86x(0x21, &inregs, &outregs, &sregs);
    char* emm = (char*) MK_FP(sregs.es, 0xa);

    if(memcmp(emm, "EMMXXXX0", 8))
    {
        // No EMS present
        return;
    }

    // Check for EMS version 4
    inregs.h.ah = 0x46;
    int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
    if ((outregs.h.al & 0xf0) < 0x40) {
        // Incorrect version
        return;
    }

    // Get the page address
    inregs.h.ah = 0x41;
    int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
    pageAddressSegment = outregs.x.bx;

    // Get the number of unallocated pages
    inregs.h.ah = 0x42;
    int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
    int numAvailablePages = outregs.x.bx;

    // Allocate the pages
    inregs.h.ah = 0x43;
    inregs.x.bx = numAvailablePages;
    int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);

    if (outregs.h.ah)
    {
        // Allocation failed
        return;
    }

    numAllocatedPages = numAvailablePages;
    allocationHandle = outregs.x.dx;

    allocationPageIndex = 0;
    allocationPageUsed = 0;

    for (int n = 0; n < NUM_MAPPABLE_PAGES; n++)
    {
        mappedPages[n] = 0xffff;
    }
    nextPageToMap = 0;

    isAvailable = true;
}

void EMSManager::Reset()
{
    allocationPageIndex = 0;
    allocationPageUsed = 0;
}

void EMSManager::Shutdown()
{
    if (isAvailable)
    {
        union REGS inregs, outregs;

        // Free allocated pages
        inregs.h.ah = 0x45;
        inregs.x.dx = allocationHandle;
        int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
    }
}

MemBlockHandle EMSManager::Allocate(size_t size)
{
    MemBlockHandle result;

    if (allocationPageIndex < numAllocatedPages)
    {
        if (size + allocationPageUsed > EMS_PAGE_SIZE)
        {
            allocationPageIndex++;
            allocationPageUsed = 0;
        }

        if (allocationPageIndex < numAllocatedPages)
        {
            result.emsPage = allocationPageIndex;
            result.emsPageOffset = allocationPageUsed;
            result.type = MemBlockHandle::EMS;
            allocationPageUsed += size;
        }
    }

    return result;
}

void* EMSManager::MapBlock(MemBlockHandle& handle)
{
    if (isAvailable && handle.type == MemBlockHandle::EMS)
    {
        // Check if this page is already mapped first
        for (int n = 0; n < NUM_MAPPABLE_PAGES; n++)
        {
            if (mappedPages[n] == handle.emsPage)
            {
                return MK_FP(pageAddressSegment + n * EMS_PAGE_SEGMENT_SPACING, handle.emsPageOffset);
            }
        }

        int mappedPageIndex = nextPageToMap;

        nextPageToMap++;
        if (nextPageToMap >= NUM_MAPPABLE_PAGES)
            nextPageToMap = 0;

        {
            union REGS inregs, outregs;
            inregs.h.ah = 0x44;
            inregs.h.al = mappedPageIndex;
            inregs.x.bx = handle.emsPage;
            inregs.x.dx = allocationHandle;
            int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
            mappedPages[mappedPageIndex] = handle.emsPage;
        }

        return MK_FP(pageAddressSegment + mappedPageIndex * EMS_PAGE_SEGMENT_SPACING, handle.emsPageOffset);
    }

    return nullptr;
}
