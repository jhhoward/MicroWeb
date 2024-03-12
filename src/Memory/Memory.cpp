#include <string.h>
#include <malloc.h>
#include "Memory.h"
#ifdef _DOS
#include <dos.h>
#include "../DOS/EMS.h"
extern EMSManager ems;
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
	int DOSavailable = _memmax() / 1024;
	snprintf(outString, 100, "Conv: Alloc: %dK Used: %dK DOS free: %dK EMS: Alloc: %dK Used: %dK Block: %dK Err: %d\n", 
			(int)(MemoryManager::pageAllocator.TotalAllocated() / 1024), 
			(int)(MemoryManager::pageAllocator.TotalUsed() / 1024), 
			DOSavailable, 
			EMSallocated, 
			EMSused,
			(int)(MemoryManager::pageBlockAllocator.TotalAllocated() / 1024),
			MemoryManager::pageAllocator.GetError());
#else
	snprintf(outString, 100, "Conv: Alloc: %dK Used: %dK Block allocation: %dK\n", 
			(int)(MemoryManager::pageAllocator.TotalAllocated() / 1024), 
			(int)(MemoryManager::pageAllocator.TotalUsed() / 1024),
			(int)(MemoryManager::pageBlockAllocator.TotalAllocated() / 1024));
#endif

}
