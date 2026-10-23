#ifndef NTPWR_NTDDK_H
#define NTPWR_NTDDK_H
/* Minimal independently authored NT x86 ABI declarations used by NTPWR. */
typedef unsigned char UCHAR, BOOLEAN, *PUCHAR;
typedef unsigned short USHORT, *PUSHORT;
typedef unsigned long ULONG, *PULONG;
typedef long LONG, *PLONG, NTSTATUS;
typedef unsigned short WCHAR;
typedef void VOID, *PVOID, *HANDLE;
typedef struct _UNICODE_STRING {
    USHORT Length, MaximumLength;
    WCHAR *Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor, SecurityQualityOfService;
} OBJECT_ATTRIBUTES;
typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex, Type, DataLength;
    UCHAR Data[1];
} KEY_VALUE_PARTIAL_INFORMATION, *PKEY_VALUE_PARTIAL_INFORMATION;
typedef struct _IO_STATUS_BLOCK {
    NTSTATUS Status;
    ULONG Information;
} IO_STATUS_BLOCK;
typedef struct _IO_STACK_LOCATION {
    UCHAR MajorFunction, MinorFunction, Flags, Control;
    union { struct { ULONG OutputBufferLength, InputBufferLength, IoControlCode; } DeviceIoControl; } Parameters;
} IO_STACK_LOCATION, *PIO_STACK_LOCATION;
typedef struct _IRP {
    UCHAR _reserved0[12];
    union { PVOID SystemBuffer; } AssociatedIrp;
    UCHAR _reserved1[8];
    IO_STATUS_BLOCK IoStatus;
    UCHAR _reserved2[64];
    struct { struct { PIO_STACK_LOCATION CurrentStackLocation; } Overlay; } Tail;
} IRP, *PIRP;
struct _DRIVER_OBJECT;
struct _DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT, *PDEVICE_OBJECT;
typedef NTSTATUS (__stdcall *NTPWR_DISPATCH)(PDEVICE_OBJECT, PIRP);
typedef VOID (__stdcall *NTPWR_UNLOAD)(PDRIVER_OBJECT);
struct _DRIVER_OBJECT {
    UCHAR _reserved0[52];
    NTPWR_UNLOAD DriverUnload;
    NTPWR_DISPATCH MajorFunction[24];
};
#define NTPWR_ASSERT(e, name) typedef char name[(e) ? 1 : -1]
#define NTPWR_OFF(type, field) __builtin_offsetof(type, field)
NTPWR_ASSERT(sizeof(ULONG)==4, ntpwr_ulong32);
NTPWR_ASSERT(sizeof(PVOID)==4, ntpwr_pointer32);
NTPWR_ASSERT(sizeof(WCHAR)==2, ntpwr_wchar16);
NTPWR_ASSERT(sizeof(UNICODE_STRING)==8, ntpwr_unicode8);
NTPWR_ASSERT(sizeof(OBJECT_ATTRIBUTES)==24, ntpwr_object_attrs24);
NTPWR_ASSERT(NTPWR_OFF(IRP,AssociatedIrp.SystemBuffer)==12, ntpwr_irp_buffer12);
NTPWR_ASSERT(NTPWR_OFF(IRP,IoStatus)==24, ntpwr_irp_status24);
NTPWR_ASSERT(NTPWR_OFF(IRP,Tail.Overlay.CurrentStackLocation)==96, ntpwr_irp_stack96);
NTPWR_ASSERT(NTPWR_OFF(IO_STACK_LOCATION,Parameters.DeviceIoControl.IoControlCode)==12, ntpwr_ioctl_code12);
NTPWR_ASSERT(NTPWR_OFF(DRIVER_OBJECT,DriverUnload)==52, ntpwr_unload52);
NTPWR_ASSERT(NTPWR_OFF(DRIVER_OBJECT,MajorFunction)==56, ntpwr_major56);
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define NT_SUCCESS(status) ((NTSTATUS)(status)>=0)
#define STATUS_SUCCESS ((NTSTATUS)0)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001UL)
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010UL)
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xC00000BBUL)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023UL)
#define IRP_MJ_CREATE 0
#define IRP_MJ_CLOSE 2
#define IRP_MJ_DEVICE_CONTROL 14
#define IO_NO_INCREMENT 0
#define FILE_DEVICE_UNKNOWN 0x22UL
#define OBJ_CASE_INSENSITIVE 0x40UL
#define KEY_QUERY_VALUE 1UL
#define REG_DWORD 4UL
#define KeyValuePartialInformation 2UL
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) __builtin_memset((p),0,(n))
#define RtlCopyMemory(d,s,n) __builtin_memcpy((d),(s),(n))
#define IoGetCurrentIrpStackLocation(irp) ((irp)->Tail.Overlay.CurrentStackLocation)
#define IoCompleteRequest(irp, priority) IofCompleteRequest((irp), (priority))
#define InitializeObjectAttributes(p,n,a,r,s) do { (p)->Length=sizeof(*(p)); (p)->RootDirectory=(r); (p)->ObjectName=(n); (p)->Attributes=(a); (p)->SecurityDescriptor=(s); (p)->SecurityQualityOfService=NULL; } while (0)
extern long __fastcall InterlockedExchange(volatile long *, long);
__declspec(dllimport) UCHAR __stdcall READ_PORT_UCHAR(PUCHAR);
__declspec(dllimport) VOID __stdcall WRITE_PORT_UCHAR(PUCHAR, UCHAR);
__declspec(dllimport) BOOLEAN __stdcall MmIsAddressValid(PVOID);
__declspec(dllimport) VOID __stdcall RtlInitUnicodeString(PUNICODE_STRING, const WCHAR *);
__declspec(dllimport) NTSTATUS __stdcall IoCreateDevice(PDRIVER_OBJECT, ULONG, PUNICODE_STRING, ULONG, ULONG, BOOLEAN, PDEVICE_OBJECT *);
__declspec(dllimport) NTSTATUS __stdcall IoCreateSymbolicLink(PUNICODE_STRING, PUNICODE_STRING);
__declspec(dllimport) VOID __stdcall IoDeleteDevice(PDEVICE_OBJECT);
__declspec(dllimport) NTSTATUS __stdcall IoDeleteSymbolicLink(PUNICODE_STRING);
__declspec(dllimport) VOID __fastcall IofCompleteRequest(PIRP, UCHAR);
__declspec(dllimport) NTSTATUS __stdcall ZwOpenKey(HANDLE *, ULONG, OBJECT_ATTRIBUTES *);
__declspec(dllimport) NTSTATUS __stdcall ZwQueryValueKey(HANDLE, PUNICODE_STRING, ULONG, PVOID, ULONG, PULONG);
__declspec(dllimport) NTSTATUS __stdcall ZwClose(HANDLE);
#endif
