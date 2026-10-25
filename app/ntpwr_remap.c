#include "../include/ntpwr_win32.h"

#define NT351_BASE  0x02160000UL
#define NT351_PATCH_RVA 0x00004272UL
#define NT351_STAMP 0x32275244UL
#define NT351_CHECK 0x00038230UL
#define NT351_SIZE  0x0002C000UL
#define NT4_BASE    0x01000000UL
#define NT4_PATCH_RVA   0x0000180CUL
#define NT4_STAMP   0x3B293B47UL
#define NT4_CHECK   0x0003D41DUL
#define NT4_SIZE    0x00033000UL

#define PROCESS_INFORMATION_CLASS 5UL
#define STATUS_LENGTH_MISMATCH 0xC0000004UL
#define MAX_QUERY_BYTES (1024UL * 1024UL)
#define SPI_NEXT_OFFSET 0x00UL
#define SPI_NAME_LENGTH 0x38UL
#define SPI_NAME_POINTER 0x3CUL
#define SPI_PROCESS_ID 0x44UL
#define SPI_MIN_SIZE 0x88UL
#define LEGACY_STUB_LENGTH 22UL
#define STUB_LENGTH 68UL
#define STUB_HANDLE_OFFSET 46UL
#define STUB_FUNCTION_OFFSET 51UL
#define STUB_JUMP_OFFSET 64UL
#define ACTION_STATUS  0
#define ACTION_PATCH   1
#define ACTION_RESTORE 2

#ifndef PAGE_EXECUTE_READWRITE
#define PAGE_EXECUTE_READWRITE 0x40UL
#endif

#ifndef PROCESS_VM_OPERATION
#define PROCESS_VM_OPERATION 0x0008UL
#endif

#ifndef PROCESS_VM_WRITE
#define PROCESS_VM_WRITE 0x0020UL
#endif

#ifndef PROCESS_VM_READ
#define PROCESS_VM_READ 0x0010UL
#endif

#ifndef MEM_RELEASE
#define MEM_RELEASE 0x8000UL
#endif

typedef LONG (WINAPI *NT_QUERY_SYSTEM_INFORMATION)(ULONG, PVOID, ULONG, PULONG);
typedef LONG (WINAPI *NT_QUERY_INFORMATION_PROCESS)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef LONG (WINAPI *NT_ALLOCATE_VIRTUAL_MEMORY)(HANDLE, PVOID *, ULONG,
                                                    ULONG *, ULONG, ULONG);
typedef LONG (WINAPI *NT_FREE_VIRTUAL_MEMORY)(HANDLE, PVOID *, ULONG *, ULONG);
typedef BOOL (WINAPI *PROTECT_EX)(HANDLE, LPVOID, DWORD, DWORD, DWORD *);
typedef BOOL (WINAPI *FLUSH_ICACHE)(HANDLE, LPCVOID, DWORD);

static HANDLE g_Log = INVALID_HANDLE_VALUE;
static HANDLE g_Stdout = INVALID_HANDLE_VALUE;
static const char g_HexDigits[] = "0123456789ABCDEF";
static int g_Action = ACTION_STATUS;
static BOOL g_Succeeded = FALSE;
static NT_ALLOCATE_VIRTUAL_MEMORY g_NtAllocate = NULL;
static PROTECT_EX g_ProtectEx = NULL;
static NT_FREE_VIRTUAL_MEMORY g_NtFree = NULL;
static FLUSH_ICACHE g_Flush = NULL;
static LPVOID g_NtDeviceIoControlFile = NULL;
static NT_QUERY_INFORMATION_PROCESS g_QueryProcess = NULL;
static BOOL g_Nt4 = FALSE;
static BOOL g_VisibleStatus = FALSE;

static void Out(const char *String)
{
    DWORD Written;
    DWORD Length = (DWORD)lstrlenA(String);
    if (g_Stdout != NULL && g_Stdout != INVALID_HANDLE_VALUE)
        WriteFile(g_Stdout, String, Length, &Written, NULL);
    if (g_Log != INVALID_HANDLE_VALUE)
        WriteFile(g_Log, String, Length, &Written, NULL);
}

static void Hex(DWORD Number)
{
    char Buffer[11];
    int I;
    Buffer[0] = '0';
    Buffer[1] = 'x';
    for (I = 0; I < 8; ++I)
        Buffer[I + 2] = g_HexDigits[(Number >> (28 - I * 4)) & 15UL];
    Buffer[10] = '\0';
    Out(Buffer);
}

static void Decimal(DWORD Number)
{
    char Buffer[12];
    int I = 10;
    Buffer[11] = 0;
    do {
        Buffer[I--] = (char)('0' + Number % 10UL);
        Number /= 10UL;
    } while (Number != 0 && I >= 0);
    Out(Buffer + I + 1);
}

static void PrintBytes(const BYTE *Data, DWORD Size)
{
    DWORD I;
    char Text[4];
    Text[0] = ' ';
    Text[3] = 0;
    for (I = 0; I < Size; ++I) {
        Text[1] = g_HexDigits[Data[I] >> 4];
        Text[2] = g_HexDigits[Data[I] & 15];
        Out(Text);
    }
}

static DWORD Dword(const BYTE *Data, DWORD Offset)
{
    return (DWORD)Data[Offset] |
           ((DWORD)Data[Offset + 1] << 8) |
           ((DWORD)Data[Offset + 2] << 16) |
           ((DWORD)Data[Offset + 3] << 24);
}

static WORD Word(const BYTE *Data, DWORD Offset)
{
    return (WORD)((WORD)Data[Offset] | ((WORD)Data[Offset + 1] << 8));
}

static void StoreDword(BYTE *Data, DWORD Offset, DWORD Value)
{
    Data[Offset] = (BYTE)Value;
    Data[Offset + 1] = (BYTE)(Value >> 8);
    Data[Offset + 2] = (BYTE)(Value >> 16);
    Data[Offset + 3] = (BYTE)(Value >> 24);
}

static BOOL SameBytes(const BYTE *Left, const BYTE *Right, DWORD Size)
{
    DWORD I;
    for (I = 0; I < Size; ++I)
        if (Left[I] != Right[I]) return FALSE;
    return TRUE;
}

static BOOL Read(HANDLE Process, DWORD Address, BYTE *Buffer, DWORD Length)
{
    DWORD Done = 0;
    SetLastError(0);
    return ReadProcessMemory(Process, (LPCVOID)Address, Buffer, Length, &Done)
           && Done == Length;
}

static BOOL WriteRemote(HANDLE Process, DWORD Address, const BYTE *Buffer,
                        DWORD Length)
{
    DWORD Done = 0;
    SetLastError(0);
    if (!WriteProcessMemory(Process, (LPVOID)Address, (LPVOID)Buffer, Length, &Done))
        return FALSE;
    return Done == Length;
}

static void BuildLegacyStub(BYTE *Stub, DWORD StubAddress,
                            DWORD PatchAddress, BOOL Nt4)
{
    static const BYTE Template351[LEGACY_STUB_LENGTH] = {
        0x8B,0x5C,0x24,0x14, 0x9C,0x83,0xFB,0x05,
        0x75,0x05,0xBB,0x0A,0,0,0,0x9D,0x53,
        0xE9,0,0,0,0
    };
    static const BYTE Template4[LEGACY_STUB_LENGTH] = {
        0x8B,0x74,0x24,0x18, 0x9C,0x83,0xFE,0x05,
        0x75,0x05,0xBE,0x0A,0,0,0,0x9D,0x56,
        0xE9,0,0,0,0
    };
    const BYTE *Template = Nt4 ? Template4 : Template351;
    DWORD I;
    for (I = 0; I < LEGACY_STUB_LENGTH; ++I) Stub[I] = Template[I];
    StoreDword(Stub, 18, (PatchAddress + 5UL) -
                            (StubAddress + LEGACY_STUB_LENGTH));
}

static void BuildStub(BYTE *Stub, DWORD StubAddress, DWORD PatchAddress,
                      BOOL Nt4, DWORD RemoteHandle, DWORD NtIoctl)
{
    static const BYTE Template[STUB_LENGTH] = {
        0x8B,0x5C,0x24,0x14,0x9C,0x83,0xFB,0x05,0x75,0x33,
        0xBB,0x0A,0,0,0,0x60,0x83,0xEC,0x10,0x8D,0x44,0x24,
        0x08,0x8D,0x0C,0x24,0x6A,0x04,0x50,0x6A,0,0x6A,0,
        0x68,0x14,0x20,0x22,0,0x51,0x6A,0,0x6A,0,0x6A,0,
        0x68,0,0,0,0,0xB8,0,0,0,0,0xFF,0xD0,0x83,0xC4,
        0x10,0x61,0x9D,0x53,0xE9,0,0,0,0
    };
    DWORD I;
    for (I = 0; I < STUB_LENGTH; ++I) Stub[I] = Template[I];
    if (Nt4) {
        Stub[1] = 0x74; Stub[3] = 0x18;
        Stub[6] = 0xFE; Stub[10] = 0xBE; Stub[62] = 0x56;
    }
    StoreDword(Stub, STUB_HANDLE_OFFSET, RemoteHandle);
    StoreDword(Stub, STUB_FUNCTION_OFFSET, NtIoctl);
    StoreDword(Stub, STUB_JUMP_OFFSET,
               (PatchAddress + 5UL) - (StubAddress + STUB_LENGTH));
}

static BOOL ReadVerifiedStub(HANDLE Process, DWORD PatchAddress,
                             const BYTE *PatchBytes, BOOL Nt4,
                             DWORD *StubAddress, DWORD *RemoteHandle,
                             BOOL *Modern)
{
    BYTE Actual[STUB_LENGTH], Expected[STUB_LENGTH];
    DWORD Target;
    if (PatchBytes[0] != 0xE9) return FALSE;
    Target = PatchAddress + 5UL + Dword(PatchBytes, 1);
    if (Target < 0x10000UL || Target >= 0x80000000UL) return FALSE;
    if (!Read(Process, Target, Actual, STUB_LENGTH)) return FALSE;

    BuildStub(Expected, Target, PatchAddress, Nt4,
              Dword(Actual, STUB_HANDLE_OFFSET),
              Dword(Actual, STUB_FUNCTION_OFFSET));
    if (SameBytes(Actual, Expected, STUB_LENGTH)) {
        *StubAddress = Target;
        *RemoteHandle = Dword(Actual, STUB_HANDLE_OFFSET);
        *Modern = TRUE;
        return TRUE;
    }

    BuildLegacyStub(Expected, Target, PatchAddress, Nt4);
    if (!SameBytes(Actual, Expected, LEGACY_STUB_LENGTH)) return FALSE;
    *StubAddress = Target;
    *RemoteHandle = 0;
    *Modern = FALSE;
    return TRUE;
}

static BOOL CloseRemoteHandle(HANDLE Process, DWORD RemoteHandle)
{
    HANDLE LocalHandle = NULL;
    BOOL Result;
    if (!RemoteHandle) return FALSE;
    Result = DuplicateHandle(Process, (HANDLE)RemoteHandle, GetCurrentProcess(),
                             &LocalHandle, 0, FALSE,
                             DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
    if (LocalHandle) CloseHandle(LocalHandle);
    return Result;
}

static BOOL ReplaceCode(HANDLE Process, DWORD Address, const BYTE *NewBytes,
                        const BYTE *ExpectedBytes, DWORD Size)
{
    BYTE Current[8];
    BYTE Verify[8];
    DWORD PreviousProtection;
    DWORD IgnoredProtection;
    DWORD ErrorCode;
    BOOL Success;
    BOOL ProtectionRestored;
    if (Size > sizeof(Current) || !g_ProtectEx || !g_Flush)
        return FALSE;
    if (!Read(Process, Address, Current, Size) ||
        !SameBytes(Current, ExpectedBytes, Size)) {
        Out("Patch site changed before write\r\n");
        return FALSE;
    }
    if (!g_ProtectEx(Process, (LPVOID)Address, Size,
                     PAGE_EXECUTE_READWRITE, &PreviousProtection)) {
        Out("VirtualProtectEx failed: "); Hex(GetLastError()); Out("\r\n");
        return FALSE;
    }
    Success = WriteRemote(Process, Address, NewBytes, Size);
    if (!Success) {
        ErrorCode = GetLastError();
        Out("WriteProcessMemory failed: "); Hex(ErrorCode); Out("\r\n");
        if (!WriteRemote(Process, Address, ExpectedBytes, Size))
            Out("ERROR: patch restoration failed\r\n");
    }
    g_Flush(Process, (LPCVOID)Address, Size);
    ProtectionRestored = g_ProtectEx(Process, (LPVOID)Address, Size,
                                     PreviousProtection, &IgnoredProtection);
    if (!ProtectionRestored) {
        Out("Protection restoration failed: "); Hex(GetLastError()); Out("\r\n");
    }
    if (!Read(Process, Address, Verify, Size) ||
        !SameBytes(Verify, Success ? NewBytes : ExpectedBytes, Size)) {
        Out("Patch verification failed\r\n");
        return FALSE;
    }
    return Success && ProtectionRestored;
}

static BOOL ExecutablePatch(HANDLE Process, DWORD Base, DWORD PeOffset,
                            DWORD OptSize, DWORD Sections, DWORD PatchRva)
{
    BYTE Section[40];
    DWORD I;
    for (I = 0; I < Sections; ++I) {
        DWORD Rva, Size;
        if (!Read(Process, Base + PeOffset + 24UL + OptSize + I * 40UL,
                  Section, sizeof(Section)))
            return FALSE;
        Rva = Dword(Section, 12);
        Size = Dword(Section, 8);
        if (Size < 5UL || PatchRva < Rva || PatchRva - Rva > Size - 5UL)
            continue;
        return (Dword(Section, 36) & 0x20000020UL) == 0x20000020UL;
    }
    return FALSE;
}

static VOID ReportWinlogonCandidates(HANDLE Process, DWORD Base,
                                    DWORD PeOffset, DWORD OptSize,
                                    DWORD SectionCount, DWORD ImageSize,
                                    const BYTE *Pattern)
{
    BYTE Header[40], Bytes[1028], Context[32];
    DWORD S, Rva, Size, Offset, N, I, Count = 0;
    for (S = 0; S < SectionCount; ++S) {
        if (!Read(Process, Base + PeOffset + 24UL + OptSize + S * 40UL,
                  Header, sizeof(Header))) continue;
        if ((Dword(Header, 36) & 0x20000020UL) != 0x20000020UL) continue;
        Rva = Dword(Header, 12);
        Size = Dword(Header, 8);
        if (Size < 5UL || Rva >= ImageSize || Size > ImageSize - Rva)
            continue;
        for (Offset = 0; Offset + 5UL <= Size; Offset += 1024UL) {
            N = Size - Offset;
            if (N > sizeof(Bytes)) N = sizeof(Bytes);
            if (!Read(Process, Base + Rva + Offset, Bytes, N)) continue;
            for (I = 0; I < 1024UL && I + 5UL <= N; ++I) {
                if (!SameBytes(Bytes + I, Pattern, 5UL)) continue;
                ++Count;
                if (Count > 8UL) continue;
                Out("Candidate RVA: "); Hex(Rva + Offset + I);
                if (Offset + I >= 16UL && Offset + I + 16UL <= Size &&
                    Read(Process, Base + Rva + Offset + I - 16UL,
                         Context, sizeof(Context))) {
                    Out(" context:");
                    PrintBytes(Context, sizeof(Context));
                }
                Out("\r\n");
            }
        }
    }
    Out("Winlogon candidates: "); Decimal(Count); Out("\r\n");
}

static BOOL CheckImage(HANDLE Process, DWORD Base, DWORD PatchRva,
                       DWORD Timestamp, DWORD Checksum, DWORD Size,
                       const BYTE *Original, DWORD Pid, BOOL Nt4,
                       const char *Name)
{
    BYTE Dos[128];
    BYTE Nt[128];
    BYTE Site[5];
    BYTE Jump[5];
    BYTE Stub[STUB_LENGTH];
    BYTE Verify[STUB_LENGTH];
    DWORD PeOffset, Patch, ImageSize, Sections, OptSize;
    BOOL Known;
    DWORD StubAddress = 0, RemoteHandle = 0;
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    BOOL IsOriginal;
    BOOL IsPatched, Modern = FALSE;
    DWORD AllocAddress;
    PVOID AllocationBase;
    ULONG AllocationSize;
    LONG AllocationStatus;
    LONG FreeStatus;

    if (!Read(Process, Base, Dos, sizeof(Dos))) return FALSE;
    if (Dos[0] != 'M' || Dos[1] != 'Z') return FALSE;
    PeOffset = Dword(Dos, 0x3c);
    if (PeOffset < 64UL || PeOffset > 0x1000UL) return FALSE;
    if (!Read(Process, Base + PeOffset, Nt, sizeof(Nt))) return FALSE;
    if (Dword(Nt, 0) != 0x00004550UL ||
        Word(Nt, 4) != 0x014CUL || Word(Nt, 24) != 0x010B)
        return FALSE;
    Sections = Word(Nt, 6);
    OptSize = Word(Nt, 20);
    ImageSize = Dword(Nt, 24 + 56);
    if (Sections == 0 || Sections > 32UL || OptSize < 0xE0UL ||
        OptSize > 0x200UL || ImageSize < 0x10000UL ||
        ImageSize > 0x400000UL)
        return FALSE;
    if (PatchRva > ImageSize - 5UL ||
        !ExecutablePatch(Process, Base, PeOffset, OptSize, Sections, PatchRva)) {
        if (g_VisibleStatus) {
            Out("Winlogon: "); Out(Name);
            Out("\r\nImage base: "); Hex(Base);
            Out("\r\nExpected patch RVA unavailable\r\n");
            ReportWinlogonCandidates(Process, Base, PeOffset, OptSize,
                                     Sections, ImageSize, Original);
        }
        return FALSE;
    }
    Known = Dword(Nt, 8) == Timestamp &&
            Dword(Nt, 24 + 64) == Checksum && ImageSize == Size;
    Patch = Base + PatchRva;

    Out("Winlogon: "); Out(Name);
    Out("\r\nPID: "); Hex(Pid);
    Out("\r\nImage base: "); Hex(Base);
    Out("\r\nPE timestamp: "); Hex(Dword(Nt, 8));
    Out("\r\nPE checksum: "); Hex(Dword(Nt, 24 + 64));
    Out("\r\nImage profile: "); Out(Known ? "verified" : "structural (unverified)");
    Out("\r\nPatch address: "); Hex(Patch);
    if (!Read(Process, Patch, Site, sizeof(Site))) {
        Out("\r\nReadProcessMemory failed: "); Hex(GetLastError());
        Out("\r\nResult: FAILED\r\n");
        return TRUE;
    }
    Out("\r\nPatch bytes:"); PrintBytes(Site, sizeof(Site)); Out("\r\n");
    IsOriginal = SameBytes(Site, Original, sizeof(Site));
    IsPatched = ReadVerifiedStub(Process, Patch, Site, Nt4,
                                 &StubAddress, &RemoteHandle, &Modern);
    if (IsPatched) {
        Out("Patch state: installed; trampoline="); Hex(StubAddress);
        Out("\r\n");
    } else if (IsOriginal) {
        Out("Patch state: original\r\n");
    } else {
        Out("Patch state: unrecognized\r\n");
        if (g_VisibleStatus)
            ReportWinlogonCandidates(Process, Base, PeOffset, OptSize,
                                     Sections, ImageSize, Original);
        Out("Result: FAILED\r\n");
        return TRUE;
    }

    if (g_Action == ACTION_STATUS) {
        g_Succeeded = TRUE;
        Out("STATUS complete.\r\n");
        return TRUE;
    }
    if (g_Action == ACTION_PATCH && IsPatched) {
        if (!Modern) {
            Out("Legacy patch installed; remove the previous installation\r\n");
            return TRUE;
        }
        g_Succeeded = TRUE;
        Out("PATCH already installed.\r\n");
        return TRUE;
    }
    if (g_Action == ACTION_RESTORE && IsOriginal) {
        g_Succeeded = TRUE;
        Out("RESTORE complete.\r\n");
        return TRUE;
    }
    if (!g_ProtectEx || !g_Flush ||
        (g_Action == ACTION_PATCH &&
         (!g_NtAllocate || !g_NtFree || !g_NtDeviceIoControlFile))) {
        Out("Required memory APIs unavailable\r\nResult: FAILED\r\n");
        return TRUE;
    }
    if (g_Action == ACTION_RESTORE) {
        if (ReplaceCode(Process, Patch, Original, Site, sizeof(Site))) {
            if (Modern && !CloseRemoteHandle(Process, RemoteHandle))
                Out("Remote handle cleanup failed\r\n");
            g_Succeeded = TRUE;
            Out("RESTORE complete.\r\n");
        } else {
            Out("RESTORE failed.\r\n");
        }
        return TRUE;
    }

    {
        BYTE RemoteCode[16];
        if (!Read(Process, (DWORD)g_NtDeviceIoControlFile,
                  RemoteCode, sizeof(RemoteCode)) ||
            !SameBytes(RemoteCode, (const BYTE *)g_NtDeviceIoControlFile,
                       sizeof(RemoteCode))) {
            Out("Winlogon NtDeviceIoControlFile mapping differs\r\n");
            return TRUE;
        }
    }
    AllocationBase = NULL;
    AllocationSize = 4096UL;
    AllocationStatus = g_NtAllocate(Process, &AllocationBase, 0UL,
                                    &AllocationSize, MEM_COMMIT | MEM_RESERVE,
                                    PAGE_EXECUTE_READWRITE);
    if (AllocationStatus < 0) {
        Out("NtAllocateVirtualMemory failed: "); Hex((DWORD)AllocationStatus);
        Out("\r\nResult: FAILED\r\n");
        return TRUE;
    }
    AllocAddress = (DWORD)AllocationBase;
    if (AllocAddress < 0x10000UL || AllocAddress >= 0x80000000UL) {
        Out("Unexpected allocation address: "); Hex(AllocAddress);
        Out("\r\nResult: FAILED\r\n");
        AllocationSize = 0UL;
        FreeStatus = g_NtFree(Process, &AllocationBase, &AllocationSize, MEM_RELEASE);
        if (FreeStatus < 0) {
            Out("NtFreeVirtualMemory failed: "); Hex((DWORD)FreeStatus);
            Out("\r\n");
        }
        return TRUE;
    }
    DeviceHandle = CreateFileA("\\\\.\\NTPWR", GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (DeviceHandle == INVALID_HANDLE_VALUE ||
        !DuplicateHandle(GetCurrentProcess(), DeviceHandle, Process,
                         (HANDLE *)&RemoteHandle, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        Out("NTPWR device handle duplication failed: ");
        Hex(GetLastError()); Out("\r\n");
        if (DeviceHandle != INVALID_HANDLE_VALUE) CloseHandle(DeviceHandle);
        AllocationBase = (PVOID)AllocAddress;
        AllocationSize = 0UL;
        g_NtFree(Process, &AllocationBase, &AllocationSize, MEM_RELEASE);
        return TRUE;
    }
    CloseHandle(DeviceHandle);
    BuildStub(Stub, AllocAddress, Patch, Nt4, RemoteHandle,
              (DWORD)g_NtDeviceIoControlFile);
    if (!WriteRemote(Process, AllocAddress, Stub, STUB_LENGTH) ||
        !Read(Process, AllocAddress, Verify, STUB_LENGTH) ||
        !SameBytes(Verify, Stub, STUB_LENGTH) ||
        !g_Flush(Process, (LPCVOID)AllocAddress, STUB_LENGTH)) {
        Out("Trampoline initialization failed: "); Hex(GetLastError());
        Out("\r\nResult: FAILED\r\n");
        CloseRemoteHandle(Process, RemoteHandle);
        AllocationBase = (PVOID)AllocAddress;
        AllocationSize = 0UL;
        FreeStatus = g_NtFree(Process, &AllocationBase, &AllocationSize, MEM_RELEASE);
        if (FreeStatus < 0) {
            Out("NtFreeVirtualMemory failed: "); Hex((DWORD)FreeStatus);
            Out("\r\n");
        }
        return TRUE;
    }
    Jump[0] = 0xE9;
    StoreDword(Jump, 1, AllocAddress - (Patch + 5UL));
    if (!ReplaceCode(Process, Patch, Jump, Original, sizeof(Jump))) {
        Out("PATCH failed.\r\n");
        /* Retain the handle if the remote trampoline could still execute. */
        return TRUE;
    }
    if (!Read(Process, Patch, Site, sizeof(Site)) ||
        !ReadVerifiedStub(Process, Patch, Site, Nt4,
                          &StubAddress, &RemoteHandle, &Modern) || !Modern) {
        Out("PATCH verification failed.\r\n");
        return TRUE;
    }
    g_Succeeded = TRUE;
    Out("PATCH installed; trampoline="); Hex(StubAddress); Out("\r\n");
    return TRUE;
}

static void DebugPrivilege(void)
{
    HANDLE Token;
    TOKEN_PRIVILEGES Privileges;
    LUID Luid;
    DWORD ErrorCode;
    Out("SeDebugPrivilege: ");
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token)) {
        Out("unavailable, error="); Hex(GetLastError()); Out("\r\n");
        return;
    }
    if (!LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &Luid)) {
        Out("lookup failed, error="); Hex(GetLastError()); Out("\r\n");
        CloseHandle(Token);
        return;
    }
    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Luid = Luid;
    Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    SetLastError(ERROR_SUCCESS);
    if (!AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, NULL, NULL)) {
        Out("adjust failed, error="); Hex(GetLastError()); Out("\r\n");
    } else {
        ErrorCode = GetLastError();
        if (ErrorCode == ERROR_SUCCESS) Out("enabled\r\n");
        else { Out("not assigned, error="); Hex(ErrorCode); Out("\r\n"); }
    }
    CloseHandle(Token);
}

static BOOL IsWinlogon(const BYTE *Start, DWORD Total, DWORD Position,
                       DWORD EntrySpan)
{
    DWORD Address;
    DWORD Length;
    DWORD I;
    const char *Name = "WINLOGON.EXE";
    const BYTE *String;

    if (EntrySpan < SPI_MIN_SIZE) return FALSE;
    Length = (DWORD)Word(Start + Position, SPI_NAME_LENGTH);
    Address = Dword(Start + Position, SPI_NAME_POINTER);
    if (Length != 24UL && Length != 16UL) return FALSE;
    if (Address < (DWORD)Start || Address > (DWORD)Start + Total ||
        Length > ((DWORD)Start + Total - Address)) return FALSE;
    String = (const BYTE *)Address;
    for (I = 0; I < Length / 2UL; ++I) {
        BYTE C = String[I * 2];
        if (String[I * 2 + 1] != 0) return FALSE;
        if (C >= 'a' && C <= 'z') C = (BYTE)(C - 32);
        if (C != (BYTE)Name[I]) return FALSE;
    }
    return TRUE;
}

static BOOL InspectPid(DWORD Pid)
{
    static const BYTE NT351Bytes[5] = {0x8B, 0x5C, 0x24, 0x14, 0x53};
    static const BYTE NT4Bytes[5] = {0x8B, 0x74, 0x24, 0x18, 0x56};
    HANDLE Process;
    DWORD Rights, Base = 0, Basic[6], Returned = 0;
    BYTE ImageBase[4];
    BOOL Found = FALSE;

    Rights = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (g_Action != ACTION_STATUS)
        Rights |= PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_DUP_HANDLE;
    Process = OpenProcess(Rights, FALSE, Pid);
    if (Process == NULL) {
        Out("OpenProcess failed: "); Hex(GetLastError());
        Out("\r\nResult: FAILED\r\n");
        return FALSE;
    }
    if (g_QueryProcess != NULL &&
        g_QueryProcess(Process, 0, Basic, sizeof(Basic), &Returned) >= 0 &&
        Basic[1] >= 0x10000UL && Basic[1] < 0x80000000UL &&
        Read(Process, Basic[1] + 8UL, ImageBase, sizeof(ImageBase)))
        Base = Dword(ImageBase, 0);
    if (Base < 0x10000UL || Base >= 0x80000000UL ||
        !Read(Process, Base, ImageBase, 2) ||
        ImageBase[0] != 'M' || ImageBase[1] != 'Z')
        Base = g_Nt4 ? NT4_BASE : NT351_BASE;
    Found = g_Nt4 ?
        CheckImage(Process, Base, NT4_PATCH_RVA, NT4_STAMP,
                   NT4_CHECK, NT4_SIZE, NT4Bytes, Pid, TRUE, "NT 4.0") :
        CheckImage(Process, Base, NT351_PATCH_RVA, NT351_STAMP,
                   NT351_CHECK, NT351_SIZE, NT351Bytes, Pid, FALSE, "NT 3.51");
    if (!Found) Out("Winlogon image: unsupported\r\n");
    CloseHandle(Process);
    return Found;
}

BOOL NtpwrRemapAction(LPCSTR Action)
{
    NT_QUERY_SYSTEM_INFORMATION Query;
    HMODULE Ntdll;
    HMODULE Kernel32;
    BYTE *Buffer = NULL;
    DWORD Capacity = 65536UL;
    DWORD Required = 0;
    DWORD Offset;
    DWORD Count = 0;
    DWORD WinlogonCount = 0;
    DWORD Next;
    DWORD Span;
    DWORD Pid;
    LONG Status = -1;
    OSVERSIONINFOA Version;
    int Attempts;
    LPCSTR CommandLine;
    BOOL Found = FALSE;

    g_Stdout = INVALID_HANDLE_VALUE;
    g_Log = INVALID_HANDLE_VALUE;
    g_Succeeded = FALSE;
    g_Action = ACTION_STATUS;
    CommandLine = Action;
    if (lstrcmpiA(CommandLine, "PATCH") == 0) g_Action = ACTION_PATCH;
    else if (lstrcmpiA(CommandLine, "RESTORE") == 0) g_Action = ACTION_RESTORE;
    else if (lstrcmpiA(CommandLine, "STATUS") != 0 &&
             lstrcmpiA(CommandLine, "VERIFY") != 0) return FALSE;
    g_VisibleStatus = lstrcmpiA(CommandLine, "STATUS") == 0;
    if (g_VisibleStatus)
        g_Stdout = GetStdHandle((DWORD)-11);

    Out("NTPWR - Winlogon remap\r\n");
    Out("Action: ");
    Out(g_Action == ACTION_PATCH ? "PATCH" :
        (g_Action == ACTION_RESTORE ? "RESTORE" : "STATUS"));
    Out("\r\n");
    ZeroMemory(&Version, sizeof(Version));
    Version.dwOSVersionInfoSize = sizeof(Version);
    if (!GetVersionExA(&Version) ||
        !((Version.dwMajorVersion == 3UL && Version.dwMinorVersion == 51UL) ||
          (Version.dwMajorVersion == 4UL && Version.dwMinorVersion == 0UL))) {
        Out("Unsupported Windows version\r\n");
        goto Done;
    }
    g_Nt4 = Version.dwMajorVersion == 4UL;
    Out("Windows version: "); Decimal(Version.dwMajorVersion);
    Out("."); Decimal(Version.dwMinorVersion);
    Out(" build="); Decimal(Version.dwBuildNumber);
    Out(" "); Out(Version.szCSDVersion); Out("\r\n");
    DebugPrivilege();
    Kernel32 = GetModuleHandleA("kernel32.dll");
    if (Kernel32 != NULL) {
        g_ProtectEx = (PROTECT_EX)GetProcAddress(Kernel32, "VirtualProtectEx");
        g_Flush = (FLUSH_ICACHE)GetProcAddress(Kernel32, "FlushInstructionCache");
    }
    Ntdll = GetModuleHandleA("ntdll.dll");
    if (Ntdll != NULL) {
        g_NtAllocate = (NT_ALLOCATE_VIRTUAL_MEMORY)GetProcAddress(
                           Ntdll, "NtAllocateVirtualMemory");
        g_NtFree = (NT_FREE_VIRTUAL_MEMORY)GetProcAddress(
                       Ntdll, "NtFreeVirtualMemory");
        g_NtDeviceIoControlFile =
            GetProcAddress(Ntdll, "NtDeviceIoControlFile");
        g_QueryProcess = (NT_QUERY_INFORMATION_PROCESS)GetProcAddress(
            Ntdll, "NtQueryInformationProcess");
    }
    Out("NtAllocateVirtualMemory: "); Out(g_NtAllocate ? "available\r\n" : "unavailable\r\n");
    Out("NtFreeVirtualMemory: "); Out(g_NtFree ? "available\r\n" : "unavailable\r\n");
    Out("VirtualProtectEx: "); Out(g_ProtectEx ? "available\r\n" : "unavailable\r\n");
    Out("FlushInstructionCache: "); Out(g_Flush ? "available\r\n" : "unavailable\r\n");

    if (g_Action == ACTION_PATCH && !g_NtDeviceIoControlFile) {
        Out("NtDeviceIoControlFile unavailable\r\n"); goto Done;
    }
    Query = Ntdll == NULL ? NULL :
        (NT_QUERY_SYSTEM_INFORMATION)GetProcAddress(Ntdll, "NtQuerySystemInformation");
    if (Query == NULL) { Out("NtQuerySystemInformation unavailable\r\n"); goto Done; }
    for (Attempts = 0; Attempts < 6; ++Attempts) {
        Buffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, Capacity);
        if (Buffer == NULL) { Out("HeapAlloc failed\r\n"); goto Done; }
        Required = 0;
        Status = Query(PROCESS_INFORMATION_CLASS, Buffer, Capacity, &Required);
        if ((DWORD)Status != STATUS_LENGTH_MISMATCH &&
            (DWORD)Status != 0xC0000023UL) break;
        HeapFree(GetProcessHeap(), 0, Buffer);
        Buffer = NULL;
        if (Capacity >= MAX_QUERY_BYTES) break;
        Capacity *= 2UL;
        if (Required > Capacity && Required <= MAX_QUERY_BYTES)
            Capacity = Required + 4096UL;
        if (Capacity > MAX_QUERY_BYTES) Capacity = MAX_QUERY_BYTES;
    }
    Out("NtQuerySystemInformation status: "); Hex((DWORD)Status);
    Out("\r\n");
    if (Status < 0 || Buffer == NULL) { Out("Process query failed\r\n"); goto Done; }

    Offset = 0;
    while (Offset + SPI_MIN_SIZE <= Capacity && Count < 8192UL) {
        Next = Dword(Buffer + Offset, SPI_NEXT_OFFSET);
        Span = (Next == 0) ? (Capacity - Offset) : Next;
        if (Span < SPI_MIN_SIZE || Span > Capacity - Offset) {
            Out("Invalid process entry\r\n"); goto Done;
        }
        ++Count;
        if (IsWinlogon(Buffer, Capacity, Offset, Span)) {
            ++WinlogonCount;
            Pid = Dword(Buffer + Offset, SPI_PROCESS_ID);
            if (InspectPid(Pid)) Found = TRUE;
        }
        if (Next == 0) break;
        Offset += Next;
    }
    Out("Processes enumerated: "); Hex(Count);
    Out("\r\nWinlogon instances: "); Hex(WinlogonCount);
    Out("\r\n");
    if (!WinlogonCount) Out("Winlogon not listed\r\n");
    if (!Found) Out("Result: FAILED\r\n");

Done:
    if (Buffer != NULL) HeapFree(GetProcessHeap(), 0, Buffer);
    if (g_Log != INVALID_HANDLE_VALUE) CloseHandle(g_Log);
    return g_Succeeded;
}
