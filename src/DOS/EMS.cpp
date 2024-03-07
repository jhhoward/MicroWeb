#include "EMS.h"
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>

#define EMS_INTERRUPT_NUMBER 0x67

void EMSManager::Init()
{
    union REGS inregs, outregs;

    // Check for EMS driver
    inregs.h.ah = 0x40;
    int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
    if (outregs.h.ah) {
        // No EMS present
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
    mappedPage = 0xffff;

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
        if (mappedPage != handle.emsPage)
        {
            union REGS inregs, outregs;
            inregs.h.ah = 0x44;
            inregs.h.al = 0;
            inregs.x.bx = handle.emsPage;
            inregs.x.dx = allocationHandle;
            int86(EMS_INTERRUPT_NUMBER, &inregs, &outregs);
            mappedPage = handle.emsPage;
        }

        return MK_FP(pageAddressSegment, handle.emsPageOffset);
    }

    return nullptr;
}
