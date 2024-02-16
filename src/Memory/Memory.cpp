#include "Memory.h"

LinearAllocator MemoryManager::pageAllocator;
LinearAllocator MemoryManager::interfaceAllocator;
MemBlockAllocator MemoryManager::pageBlockAllocator;
