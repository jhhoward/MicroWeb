#include "XMS.h"
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(1)
typedef union 
{
    uint32_t xmsAddress;
    void* ptr;
} XMSAddr;

static struct XMSMove 
{
    uint32_t length;
    uint16_t srcHandle;
    XMSAddr  src;       
    uint16_t destHandle;
    XMSAddr  dest;      
} g_XMSMove;
#pragma pack()

void (*XMSEntry)() = NULL;

// Returns 1 on success, 0 on failure. Error code is in BL if AX is 0.
extern uint16_t XMSTransferCall(void);

#pragma aux XMSTransferCall = \
    "mov ax, seg [XMSEntry]" \
    "mov es, ax" \
    "mov bx, offset [XMSEntry]" \
    "push ds" \
    "mov si, offset [g_XMSMove]" \
    "mov ax, seg [g_XMSMove]" \
    "mov ds, ax" \
    "mov ah, 0bh" \
    "call dword ptr es:[bx]" \
    "pop ds" \
    value [ax] \
    modify [ax bx es si];

// Returns the handle (DX) or 0 on failure
extern uint16_t XMSAllocate(uint16_t kb);

#pragma aux XMSAllocate = \
    "mov ah, 09h"     \
    "call dword ptr [XMSEntry]" \
    "cmp ax, 1"       /* Check success (AX=1) */ \
    "je success"      \
    "xor dx, dx"      /* If AX != 1, return 0 in DX */ \
    "success:"        \
    parm [dx]         \
    value [dx]        /* Return handle in DX */ \
    modify [ax dx];   

extern void XMSFree(uint16_t handle);

#pragma aux XMSFree = \
    "test dx, dx"     /* Don't call if handle is 0 */ \
    "jz skip"         \
    "mov ah, 0Ah"     \
    "call dword ptr [XMSEntry]" \
    "skip:"           \
    parm [dx]         \
    modify [ax];      /* Driver returns result in AX, we ignore it */

// Returns the largest contiguous XMS block available (in KB)
extern uint16_t XMSGetLargestFree(void);

#pragma aux XMSGetLargestFree = \
    "mov ah, 08h"     \
    "call dword ptr [XMSEntry]" \
    /* AX now contains largest block in KB */ \
    /* DX contains total free XMS in KB (ignored here) */ \
    value [ax]        \
    modify [ax dx];

void XMSManager::Init()
{
    totalUsed = 0;
    totalAllocated = 0;
    isAvailable = false;

    union REGS r;
    struct SREGS s;

    // Check for driver presence
    r.x.ax = 0x4300;
    int86(0x2f, &r, &r);
    if (r.h.al == 0x80)
    {
        // Get entry point
        r.x.ax = 0x4310;
        int86x(0x2f, &r, &r, &s);
        XMSEntry = (void (far*)())MK_FP(s.es, r.x.bx);

        if (XMSEntry != NULL)
        {
            uint16_t largestFree = XMSGetLargestFree();

            if (largestFree > MAX_XMS_ALLOCATION_KB)
            {
                largestFree = MAX_XMS_ALLOCATION_KB;
            }

            if (largestFree > 0)
            {
                allocationHandle = XMSAllocate(largestFree);

                if (allocationHandle)
                {
                    totalAllocated = largestFree * 1024L;

                    buffer = malloc(XMS_BUFFER_SIZE);
                    isAvailable = buffer != NULL;

                    /*
                    printf("Allocating..\n");
                    getchar();

                    char* testString = "Hello world";
                    MemBlockHandle test = Allocate(strlen(testString) + 1);
                    if (test.IsAllocated())
                    {
                        printf("Allocated from XMS\n");
                        getchar();
                        void* ptr = MapBlock(test);
                        if (ptr)
                        {
                            printf("Copying to buffer\n");
                            getchar();
                            memcpy(ptr, testString, strlen(testString) + 1);

                            printf("Committing\n");
                            getchar();
                            Commit(test);
                            memset(buffer, 0, XMS_BUFFER_SIZE);

                            printf("Reading back\n");
                            getchar();
                            ptr = MapBlock(test);

                            printf("Got: %s\n", (char*)ptr);
                            getchar();
                        }
                    }

                    getchar();
                    */
                }
            }
        }
    }
}

void XMSManager::Reset()
{
    totalUsed = 0;
}

void XMSManager::Shutdown()
{
    if (isAvailable)
    {
        XMSFree(allocationHandle);
    }
}

MemBlockHandle XMSManager::Allocate(size_t size)
{
    MemBlockHandle result;

    // Pad to 4 byte boundary
    size = (size + 3) & 0xfffc;

    if (size < XMS_BUFFER_SIZE && totalUsed + size < totalAllocated)
    {
        result.type = MemBlockHandle::XMS;
        result.xmsPointer = totalUsed >> 2;
        result.xmsLength = size >> 2;
        totalUsed += size;
    }

    return result;
}

void* XMSManager::MapBlock(MemBlockHandle& handle)
{
    g_XMSMove.length = handle.xmsLength << 2;
    g_XMSMove.destHandle = 0;
    g_XMSMove.dest.ptr = buffer;
    g_XMSMove.srcHandle = allocationHandle;
    g_XMSMove.src.xmsAddress = handle.xmsPointer << 2;

    if (XMSTransferCall())
    {
        return buffer;
    }

    return nullptr;
}

void XMSManager::Commit(MemBlockHandle& handle)
{
    g_XMSMove.length = handle.xmsLength << 2;
    g_XMSMove.srcHandle = 0;
    g_XMSMove.src.ptr = buffer;
    g_XMSMove.destHandle = allocationHandle;
    g_XMSMove.dest.xmsAddress = handle.xmsPointer << 2;

    XMSTransferCall();
}
