#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
case "$ROOT/" in */.Trash/*|*/.Trashes/*) echo "Refusing to build in Trash" >&2; exit 1;; esac
CC="${CLANG:-clang}"
LD="${LLD_LINK:-lld-link}"
if ! command -v "$LD" >/dev/null 2>&1 && command -v brew >/dev/null 2>&1; then
    LD="$(brew --prefix lld)/bin/lld-link"
fi
for tool in git python3 "$CC" "$LD"; do
    command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 1; }
done
mkdir -p "$ROOT/build" "$ROOT/DIST"
TMP=""
cleanup() { [[ -z "$TMP" ]] || rm -rf -- "$TMP"; }
trap cleanup EXIT
if [[ -n "${UACPI_SOURCE:-}" ]]; then
    SRC="$(cd "$UACPI_SOURCE" && pwd -P)"
else
    TMP="$(mktemp -d "${TMPDIR:-/tmp}/ntpwr.XXXXXX")"
    SRC="$TMP/uACPI"
    echo "Fetching uACPI 7.0.0..."
    git -c advice.detachedHead=false clone --quiet --depth 1 --branch 7.0.0 \
        https://github.com/uACPI/uACPI.git "$SRC"
fi
if [[ ! -d "$SRC/.git" || "$(git -C "$SRC" describe --tags --exact-match HEAD 2>/dev/null || true)" != '7.0.0' ]]; then
    echo 'Expected upstream uACPI tag 7.0.0.' >&2; exit 1
fi
EXPECTED_UACPI_SHA=686a9a52d9ad72690406d7cada82801377d5e2fa
ACTUAL_UACPI_SHA="$(git -C "$SRC" rev-parse HEAD)"
if [[ "$ACTUAL_UACPI_SHA" != "$EXPECTED_UACPI_SHA" ]]; then
    echo "Unexpected uACPI commit: $ACTUAL_UACPI_SHA" >&2
    exit 1
fi
RDIR="$("$CC" -print-resource-dir)"
FLAGS=(--target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2
   -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector
   -fms-extensions -O1 -nostdinc -isystem "$RDIR/include"
   -DUACPI_NATIVE_MMIO)
B="$ROOT/build"
LINK_ROOT="$ROOT"
LINK_B="$B"
if [[ "${OSTYPE:-}" == msys* || -n "${MSYSTEM:-}" ]]; then
    LINK_ROOT="$(cygpath -m "$ROOT")"
    LINK_B="$(cygpath -m "$B")"
fi
python3 "$ROOT/tools/version_resource.py" "$ROOT/include/ntpwr_version.h" "$B/ntpwr-sys.res" sys
python3 "$ROOT/tools/version_resource.py" "$ROOT/include/ntpwr_version.h" "$B/ntpwr-exe.res" exe
ADAPTER=(nt351_early nt351_sync_timing nt351_io nt351_irq_spin
         nt351_pci nt351_work nt351_firmware_irq nt351_mmio_ro)
"$CC" "${FLAGS[@]}" -c "$ROOT/runtime/nt351_runtime.c" -o "$B/runtime.obj"
for module in "${ADAPTER[@]}"; do
    "$CC" "${FLAGS[@]}" -I "$SRC/include" -I "$ROOT/adapter" \
        -c "$ROOT/adapter/$module.c" -o "$B/$module.obj"
done
"$CC" "${FLAGS[@]}" -I "$SRC/include" -I "$ROOT/adapter" \
    -c "$ROOT/driver/ntpwr_drv.c" -o "$B/ntpwr_drv.obj"
"$CC" --target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2 -O1 -ffreestanding -fno-builtin -fno-stack-protector -fms-extensions -Xclang -fdefault-calling-conv=stdcall \
    -I "$ROOT/driver" -c "$ROOT/driver/ntpwr_hook.c" -o "$B/ntpwr_hook.obj"
"$CC" "${FLAGS[@]}" -c "$ROOT/driver/nt351_interlocked.c" -o "$B/interlocked.obj"
SOURCES=(tables types uacpi utilities interpreter opcodes namespace stdlib
  opregion default_handlers io notify sleep registers resources event mutex osi)
UACPI_S5_SOURCE="$B/ntpwr_uacpi.c"
python3 - "$SRC/source/uacpi.c" "$UACPI_S5_SOURCE" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
start = "    ret = uacpi_initialize_events();\n"
end = "    g_uacpi_rt_ctx.init_level = UACPI_INIT_LEVEL_NAMESPACE_LOADED;\n"
begin = source.find(start)
finish = source.find(end, begin)
if begin < 0 or finish < 0 or source.find(start, begin + len(start)) >= 0:
    raise SystemExit("Unsupported uACPI namespace initialization")
block = source[begin:finish]
if 'goto out_fatal_error;' not in block:
    raise SystemExit("Unexpected uACPI event initialization")
source = source[:begin] + "#ifndef NTPWR_S5_ONLY\n" + block + "#endif\n" + source[finish:]
Path(sys.argv[2]).write_text(source)
PY
UACPI_REGISTER_SOURCE="$B/ntpwr_registers.c"
python3 - "$SRC/source/registers.c" "$UACPI_REGISTER_SOURCE" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
marker = "static uacpi_status get_reg(\n"
if source.count(marker) != 1:
    raise SystemExit("Unsupported uACPI register mapping")
source = source.replace(marker, """uacpi_status ntpwr_uacpi_prepare_smi_cmd(void)
{
    return ensure_register_mapped(&g_registers[UACPI_REGISTER_SMI_CMD],
                                  &g_register_mappings[UACPI_REGISTER_SMI_CMD]);
}

""" + marker)
Path(sys.argv[2]).write_text(source)
PY
UACPI_SLEEP_SOURCE="$B/ntpwr_sleep.c"
python3 - "$SRC/source/sleep.c" "$UACPI_SLEEP_SOURCE" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
header = "#include <uacpi/platform/arch_helpers.h>\n"
start = source.find("uacpi_status uacpi_prepare_for_sleep_state(")
end = source.find("static uacpi_u8 make_hw_reduced_sleep_control(", start)
if source.count(header) != 1 or start < 0 or end < 0:
    raise SystemExit("Unsupported uACPI S5 preparation source")
section = source[start:end]
original_type = """    if (ret != UACPI_STATUS_OK)
        return ret;

    ret = get_slp_type_for_state(
        0,"""
traced_type = """    if (ret != UACPI_STATUS_OK) {
        if (state == UACPI_SLEEP_STATE_S5)
            NtpwrTraceAcpiStatus("S5 type evaluation", (unsigned long)ret);
        return ret;
    }

    ret = get_slp_type_for_state(
        0,"""
original_pts = """    ret = eval_pts(state);
    if (uacpi_unlikely_error(ret))
        return ret;"""
traced_pts = """    ret = eval_pts(state);
    if (uacpi_unlikely_error(ret)) {
        if (state == UACPI_SLEEP_STATE_S5)
            NtpwrTraceAcpiStatus("S5 _PTS evaluation", (unsigned long)ret);
        return ret;
    }"""
if section.count(original_type) != 1 or section.count(original_pts) != 1:
    raise SystemExit("Unsupported uACPI S5 preparation layout")
section = section.replace(original_type, traced_type).replace(original_pts, traced_pts)
source = source[:start] + section + source[end:]
source = source.replace(header, header +
    "\nextern void __cdecl NtpwrTraceAcpiStatus(const char *, unsigned long);\n")
Path(sys.argv[2]).write_text(source)
PY
UOBJS=()
for name in "${SOURCES[@]}"; do
    unit="$SRC/source/$name.c"
    if [[ "$name" == uacpi ]]; then unit="$UACPI_S5_SOURCE"; fi
    if [[ "$name" == registers ]]; then unit="$UACPI_REGISTER_SOURCE"; fi
    if [[ "$name" == sleep ]]; then unit="$UACPI_SLEEP_SOURCE"; fi
    "$CC" "${FLAGS[@]}" -DNTPWR_S5_ONLY -I "$SRC/include" -I "$SRC/source" \
        -c "$unit" -o "$B/$name.obj"
    UOBJS+=("$LINK_B/$name.obj")
    printf 'PASS  %s.c\n' "$name"
done
cat > "$B/NTOSKRNL.DEF" <<'DEF'
LIBRARY NTOSKRNL.EXE
EXPORTS
    DbgPrint
    ExAllocatePoolWithTag
    ExFreePool
    MmMapIoSpace
    MmUnmapIoSpace
    KeInitializeSemaphore
    KeReleaseSemaphore
    KeWaitForSingleObject
    KeGetCurrentThread
    KeDelayExecutionThread
    ExQueueWorkItem
    ZwOpenKey
    ZwSetValueKey
    ZwClose
    ZwQueryValueKey
    IoCreateDevice
    IoCreateSymbolicLink
    IoDeleteDevice
    IoDeleteSymbolicLink
    MmIsAddressValid
    RtlInitUnicodeString
    IofCompleteRequest
DEF
cat > "$B/HAL.DEF" <<'DEF'
LIBRARY HAL.DLL
EXPORTS
    KeQueryPerformanceCounter
    KeStallExecutionProcessor
    READ_PORT_UCHAR
    READ_PORT_USHORT
    READ_PORT_ULONG
    WRITE_PORT_UCHAR
    WRITE_PORT_USHORT
    WRITE_PORT_ULONG
    HalGetBusDataByOffset
    HalSetBusDataByOffset
DEF
cat > "$B/KERNEL32.DEF" <<'DEF'
LIBRARY KERNEL32.DLL
EXPORTS
    GetStdHandle
    WriteFile
    FreeConsole
    GetLastError
    ExitProcess
    GetCommandLineA
    GetModuleFileNameA
    GetSystemDirectoryA
    CopyFileA
    DeleteFileA
    GetFileAttributesA
    SetFileAttributesA
    Sleep
    GetModuleHandleA
    LoadLibraryA
    FreeLibrary
    GetVersionExA
    CloseHandle
    CreateFileA
    DeviceIoControl
    GetCurrentProcess
    GetProcAddress
    GetProcessHeap
    HeapAlloc
    HeapFree
    OpenProcess
    DuplicateHandle
    ReadProcessMemory
    WriteProcessMemory
    CreateEventA
    ResetEvent
    SetEvent
    WaitForSingleObject
    SetLastError
    lstrcmpiA
    lstrlenA
DEF
cat > "$B/ADVAPI32.DEF" <<'DEF'
LIBRARY ADVAPI32.DLL
EXPORTS
    OpenSCManagerA
    CreateServiceA
    OpenServiceA
    StartServiceA
    QueryServiceStatus
    ControlService
    DeleteService
    CloseServiceHandle
    RegOpenKeyExA
    RegQueryValueExA
    RegCloseKey
    ChangeServiceConfigA
    RegCreateKeyExA
    RegSetValueExA
    OpenProcessToken
    LookupPrivilegeValueA
    AdjustTokenPrivileges
    RegisterEventSourceA
    DeregisterEventSource
    ReportEventA
    RegisterServiceCtrlHandlerA
    SetServiceStatus
    StartServiceCtrlDispatcherA
DEF
cat > "$B/USER32.DEF" <<'DEF'
LIBRARY USER32.DLL
EXPORTS
    DialogBoxIndirectParamA
    EndDialog
    SetDlgItemTextA
    SendDlgItemMessageA
    GetDlgItem
    EnableWindow
    MessageBoxA
DEF
for name in NTOSKRNL HAL KERNEL32 ADVAPI32 USER32; do
    MSYS2_ARG_CONV_EXCL="*" "$LD" /nologo /machine:x86 /dll /noentry \
        "/def:$LINK_B/${name}.DEF" "/out:$LINK_B/${name}.stub.dll"
done
imports=(ExAllocatePoolWithTag:12 ExFreePool:4 MmMapIoSpace:16
         MmUnmapIoSpace:8 KeInitializeSemaphore:12 KeReleaseSemaphore:16
         KeWaitForSingleObject:20 KeGetCurrentThread:0 KeDelayExecutionThread:12
         ExQueueWorkItem:8 ZwOpenKey:12 ZwSetValueKey:24 ZwClose:4
         ZwQueryValueKey:24 IoCreateDevice:28 IoCreateSymbolicLink:8
         IoDeleteDevice:4 IoDeleteSymbolicLink:4 MmIsAddressValid:4
         RtlInitUnicodeString:8)
aliases=()
for import in "${imports[@]}"; do
    name="${import%:*}"; size="${import#*:}"
    aliases+=("/alternatename:__imp__${name}@${size}=__imp__${name}")
done
for import in READ_PORT_UCHAR:4 READ_PORT_USHORT:4 READ_PORT_ULONG:4 \
              WRITE_PORT_UCHAR:8 WRITE_PORT_USHORT:8 WRITE_PORT_ULONG:8 \
              KeQueryPerformanceCounter:4 KeStallExecutionProcessor:4 \
              HalGetBusDataByOffset:24 HalSetBusDataByOffset:24; do
    name="${import%:*}"; size="${import#*:}"
    aliases+=("/alternatename:__imp__${name}@${size}=__imp__${name}")
done
# Fastcall import is explicitly declared as such in the original DDK.
aliases+=("/alternatename:__imp_@IofCompleteRequest@8=__imp__IofCompleteRequest")
AOBJS=()
for module in "${ADAPTER[@]}"; do AOBJS+=("$LINK_B/$module.obj"); done
MSYS2_ARG_CONV_EXCL="*" "$LD" /nologo /dll /driver /machine:x86 /subsystem:native,3.51 \
   /osversion:3.51 /entry:DriverEntry /nodefaultlib /dynamicbase:no \
   /nxcompat:no /safeseh:no /base:0x10000 /release \
   "/out:$LINK_ROOT/DIST/NTPWR.SYS" "$LINK_B/ntpwr_drv.obj" "$LINK_B/runtime.obj" \
   "${AOBJS[@]}" "$LINK_B/ntpwr_hook.obj" "$LINK_B/interlocked.obj" "${UOBJS[@]}" "$LINK_B/ntpwr-sys.res" "$LINK_B/NTOSKRNL.stub.lib" "$LINK_B/HAL.stub.lib" "${aliases[@]}"

"$CC" --target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2 \
  -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror \
  -c "$ROOT/app/ntpwr.c" -o "$B/ntpwr_cli.obj"
for unit in ntpwr_remap ntpwr_service; do
    "$CC" --target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2 -O1 -ffreestanding -fno-builtin -fno-stack-protector -fms-extensions -c "$ROOT/app/$unit.c" -o "$B/$unit.obj"
done
user_imports=(GetStdHandle:4 WriteFile:20 FreeConsole:0 GetLastError:0 ExitProcess:4
  GetCommandLineA:0 GetModuleFileNameA:12 GetSystemDirectoryA:8
  CopyFileA:12 DeleteFileA:4 GetFileAttributesA:4 SetFileAttributesA:8
  Sleep:4 GetModuleHandleA:4 LoadLibraryA:4 FreeLibrary:4 GetVersionExA:4
  CloseHandle:4 CreateFileA:28 DeviceIoControl:32 GetCurrentProcess:0
  GetProcAddress:8 GetProcessHeap:0 HeapAlloc:12 HeapFree:12 OpenProcess:12
  DuplicateHandle:28
  ReadProcessMemory:20 WriteProcessMemory:20 CreateEventA:16 ResetEvent:4
  SetEvent:4 WaitForSingleObject:8 SetLastError:4 lstrcmpiA:8 lstrlenA:4
  DialogBoxIndirectParamA:20 EndDialog:8 SetDlgItemTextA:12 SendDlgItemMessageA:20 GetDlgItem:8 EnableWindow:8 MessageBoxA:16
  OpenSCManagerA:12 CreateServiceA:52
  OpenServiceA:12 StartServiceA:12 QueryServiceStatus:8 ControlService:12
  DeleteService:4 CloseServiceHandle:4 RegOpenKeyExA:20 RegQueryValueExA:24
  RegCloseKey:4 ChangeServiceConfigA:44 RegCreateKeyExA:36 RegSetValueExA:24
  OpenProcessToken:12 LookupPrivilegeValueA:12 AdjustTokenPrivileges:24
  RegisterEventSourceA:8 DeregisterEventSource:4 ReportEventA:36
  RegisterServiceCtrlHandlerA:8 SetServiceStatus:8
  StartServiceCtrlDispatcherA:4)
user_aliases=()
for imp in "${user_imports[@]}"; do
  name="${imp%:*}"; sz="${imp#*:}"
  user_aliases+=("/alternatename:__imp__${name}@${sz}=__imp__${name}")
done
MSYS2_ARG_CONV_EXCL="*" "$LD" /nologo /machine:x86 /subsystem:console,3.51 /osversion:3.51 \
  /entry:Entry /nodefaultlib /dynamicbase:no /nxcompat:no /safeseh:no \
  "/out:$LINK_ROOT/DIST/NTPWR.EXE" "$LINK_B/ntpwr_cli.obj" \
  "$LINK_B/ntpwr_remap.obj" "$LINK_B/ntpwr_service.obj" "$LINK_B/runtime.obj" "$LINK_B/ntpwr-exe.res" \
  "$LINK_B/KERNEL32.stub.lib" "$LINK_B/ADVAPI32.stub.lib" "$LINK_B/USER32.stub.lib" "${user_aliases[@]}"
python3 "$ROOT/tools/check_pe.py" "$ROOT/DIST/NTPWR.SYS" "$ROOT/DIST/NTPWR.EXE"
echo "Build complete: DIST/NTPWR.SYS DIST/NTPWR.EXE"
