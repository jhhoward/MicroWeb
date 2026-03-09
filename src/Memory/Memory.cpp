#include <string.h>
#include <malloc.h>
#include "Memory.h"
#ifdef _DOS
#include <dos.h>
#include "../DOS/EMS.h"
extern EMSManager ems;

unsigned int GetMemFreeKB(void);

#pragma aux GetMemFreeKB = \
    "mov ah, 48h"    /* DOS: Allocate Memory Block */ \
    "mov bx, 0FFFFh" /* Request impossible amount */ \
    "int 21h"        /* Call DOS; BX returns max paragraphs */ \
    "mov ax, bx"     /* Move paragraphs to AX */ \
    "mov cl, 6"      /* Prepare to shift by 6 bits */ \
    "shr ax, cl"     /* AX = AX / 64 (converts paragraphs to KB) */ \
    value [ax]       \
    modify [ax bx cl]; 

#endif

LinearAllocator MemoryManager::pageAllocator;
MallocWrapper MemoryManager::interfaceAllocator;
MemBlockAllocator MemoryManager::pageBlockAllocator;

void MemoryManager::GenerateMemoryReport(char* outString)
{
#ifdef _DOS
//	int DOSavailable = 0;
//	union REGS inreg, outreg;
//	inreg.x.ax = 0x4800;
//	inreg.x.bx = 0;
//	intdos(&inreg, &outreg);
//	DOSavailable = outreg.x.bx / 64;
	int EMSallocated = ems.TotalAllocated() / 1024;
	int EMSused = ems.TotalUsed() / 1024;
	int DOSavailable = GetConventionalMemoryAvailableKB();
	int swapUsed = MemoryManager::pageBlockAllocator.SwapAllocated() / 1024;
	snprintf(outString, 100, "Conv: Alloc: %dK Used: %dK DOS free: %dK EMS: Alloc: %dK Used: %dK Block: %dK Swap: %dK Err: %d\n", 
			(int)(MemoryManager::pageAllocator.TotalAllocated() / 1024), 
			(int)(MemoryManager::pageAllocator.TotalUsed() / 1024), 
			DOSavailable, 
			EMSallocated, 
			EMSused,
			(int)(MemoryManager::pageBlockAllocator.TotalAllocated() / 1024),
			swapUsed,
			MemoryManager::pageAllocator.GetError());
#else
	snprintf(outString, 100, "Conv: Alloc: %dK Used: %dK Block allocation: %dK\n", 
			(int)(MemoryManager::pageAllocator.TotalAllocated() / 1024), 
			(int)(MemoryManager::pageAllocator.TotalUsed() / 1024),
			(int)(MemoryManager::pageBlockAllocator.TotalAllocated() / 1024));
#endif

}

long MemoryManager::GetConventionalMemoryAvailableKB()
{
#ifdef _DOS
	return GetMemFreeKB();
#else
	// Placeholder for other platforms - 1MB free
	return 1024;
#endif
}
