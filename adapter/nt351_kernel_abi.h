#ifndef NT351_KERNEL_ABI_H
#define NT351_KERNEL_ABI_H

/* Minimal declarations for the exact NT 3.51 exports used by this audit.
 * Production integration must use the original DDK 3.51 NTDDK.H definitions.
 */
typedef unsigned char NT351_U8;
typedef unsigned long NT351_U32;
typedef unsigned long long NT351_U64;
typedef long NT351_LONG;
typedef struct NT351_PHYSICAL_ADDRESS {
    NT351_U32 LowPart;
    NT351_LONG HighPart;
} NT351_PHYSICAL_ADDRESS;

#define NT351_NONPAGED_POOL 0UL
#define NT351_POOL_TAG 0x4D4C4155UL /* UALM */

__declspec(dllimport) void *__stdcall ExAllocatePoolWithTag(
    NT351_U32 PoolType, NT351_U32 NumberOfBytes, NT351_U32 Tag);
__declspec(dllimport) void __stdcall ExFreePool(void *Ptr);
__declspec(dllimport) void *__stdcall MmMapIoSpace(
    NT351_PHYSICAL_ADDRESS PhysicalAddress, NT351_U32 NumberOfBytes, NT351_U32 CacheEnable);
__declspec(dllimport) void __stdcall MmUnmapIoSpace(void *VirtualAddress, NT351_U32 NumberOfBytes);
__declspec(dllimport) NT351_U32 __cdecl DbgPrint(const char *Format, ...);

/* DDK 3.51 kernel declarations reproduced for ABI audit only. */
#ifndef NT351_ASSERT
#define NT351_ASSERT(x, name) typedef char name[(x) ? 1 : -1]
#endif
typedef struct NT351_LIST_ENTRY {
    struct NT351_LIST_ENTRY *Flink;
    struct NT351_LIST_ENTRY *Blink;
} NT351_LIST_ENTRY;

typedef struct NT351_DISPATCHER_HEADER {
    NT351_U8 Type;
    NT351_U8 Spare;
    unsigned short Size;
    NT351_LONG SignalState;
    NT351_LIST_ENTRY WaitListHead;
} NT351_DISPATCHER_HEADER;

typedef struct NT351_KSEMAPHORE {
    NT351_DISPATCHER_HEADER Header;
    NT351_LONG Limit;
} NT351_KSEMAPHORE;

typedef union NT351_LARGE_INTEGER {
    struct { NT351_U32 LowPart; NT351_LONG HighPart; } Parts;
    long long QuadPart;
} NT351_LARGE_INTEGER;

NT351_ASSERT(sizeof(NT351_DISPATCHER_HEADER) == 16, nt351_dispatcher_header_16);
NT351_ASSERT(sizeof(NT351_KSEMAPHORE) == 20, nt351_semaphore_20);
NT351_ASSERT(sizeof(NT351_LARGE_INTEGER) == 8, nt351_large_integer_8);

__declspec(dllimport) void __stdcall KeInitializeSemaphore(
    NT351_KSEMAPHORE *Semaphore, NT351_LONG Count, NT351_LONG Limit);
__declspec(dllimport) NT351_LONG __stdcall KeReleaseSemaphore(
    NT351_KSEMAPHORE *Semaphore, NT351_LONG Increment, NT351_LONG Adjustment,
    NT351_U8 Wait);
__declspec(dllimport) NT351_LONG __stdcall KeWaitForSingleObject(
    void *Object, NT351_U32 WaitReason, NT351_U32 WaitMode, NT351_U8 Alertable,
    NT351_LARGE_INTEGER *Timeout);
__declspec(dllimport) void *__stdcall KeGetCurrentThread(void);
__declspec(dllimport) NT351_LONG __stdcall KeDelayExecutionThread(
    NT351_U32 WaitMode, NT351_U8 Alertable, NT351_LARGE_INTEGER *Interval);

/* Exported by HAL.DLL in the original NT 3.51 DDK import library. */
__declspec(dllimport) NT351_LARGE_INTEGER __stdcall KeQueryPerformanceCounter(
    NT351_LARGE_INTEGER *PerformanceFrequency);
__declspec(dllimport) void __stdcall KeStallExecutionProcessor(NT351_U32 MicroSeconds);


/* HAL port I/O functions are declared by NTDDK.H on NT 3.51 x86. */
__declspec(dllimport) NT351_U8 __stdcall READ_PORT_UCHAR(NT351_U8 *Port);
__declspec(dllimport) unsigned short __stdcall READ_PORT_USHORT(unsigned short *Port);
__declspec(dllimport) NT351_U32 __stdcall READ_PORT_ULONG(NT351_U32 *Port);
__declspec(dllimport) void __stdcall WRITE_PORT_UCHAR(NT351_U8 *Port, NT351_U8 Value);
__declspec(dllimport) void __stdcall WRITE_PORT_USHORT(unsigned short *Port, unsigned short Value);
__declspec(dllimport) void __stdcall WRITE_PORT_ULONG(NT351_U32 *Port, NT351_U32 Value);


/* Windows NT 3.51 DDK worker queue and PCI bus interfaces. */
typedef void (__stdcall *NT351_WORK_ROUTINE)(void *);
typedef struct NT351_WORK_QUEUE_ITEM {
    NT351_LIST_ENTRY List;
    NT351_WORK_ROUTINE WorkerRoutine;
    void *Parameter;
} NT351_WORK_QUEUE_ITEM;
NT351_ASSERT(sizeof(NT351_WORK_QUEUE_ITEM) == 16, nt351_work_item_16);

__declspec(dllimport) void __stdcall ExQueueWorkItem(
    NT351_WORK_QUEUE_ITEM *Item, NT351_U32 QueueType);

/* PCIConfiguration = 4 in DDK 3.51 BUS_DATA_TYPE. */
#define NT351_PCI_CONFIGURATION 4UL
__declspec(dllimport) NT351_U32 __stdcall HalGetBusDataByOffset(
    NT351_U32 BusDataType, NT351_U32 BusNumber, NT351_U32 SlotNumber,
    void *Buffer, NT351_U32 Offset, NT351_U32 Length);
__declspec(dllimport) NT351_U32 __stdcall HalSetBusDataByOffset(
    NT351_U32 BusDataType, NT351_U32 BusNumber, NT351_U32 SlotNumber,
    void *Buffer, NT351_U32 Offset, NT351_U32 Length);

#endif
