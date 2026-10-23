#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
CC="${CLANG:-clang}"
LD="${LLD_LINK:-lld-link}"
if ! command -v "$LD" >/dev/null 2>&1 && command -v brew >/dev/null 2>&1; then
  LD="$(brew --prefix lld)/bin/lld-link"
fi
for tool in "$CC" "$LD" python3; do
  command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 1; }
done
mkdir -p "$ROOT/build" "$ROOT/DIST"
B="$ROOT/build"
"$CC" --target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2 \
  -ffreestanding -fno-builtin -fno-stack-protector -O1 -Wall -Wextra -Werror \
  -c "$ROOT/app/ntpwr.c" -o "$B/ntpwr_cli.obj"
for unit in ntpwr_remap ntpwr_service; do
  "$CC" --target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2 -O1 -ffreestanding -fno-builtin -fno-stack-protector -fms-extensions -c "$ROOT/app/$unit.c" -o "$B/$unit.obj"
done
"$CC" --target=i386-pc-windows-msvc -march=i486 -mno-sse -mno-sse2 \
  -ffreestanding -fno-builtin -fno-stack-protector -O1 \
  -c "$ROOT/runtime/nt351_runtime.c" -o "$B/runtime.obj"
cat > "$B/KERNEL32.DEF" <<'DEF'
LIBRARY KERNEL32.DLL
EXPORTS
    GetStdHandle
    WriteFile
    GetLastError
    ExitProcess
    GetCommandLineA
    GetModuleFileNameA
    GetSystemDirectoryA
    CopyFileA
    DeleteFileA
    Sleep
    GetModuleHandleA
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
    GetDlgItem
    EnableWindow
    MessageBoxA
DEF
for name in KERNEL32 ADVAPI32 USER32; do
    "$LD" /nologo /machine:x86 /dll /noentry \
        "/def:$B/${name}.DEF" "/out:$B/${name}.stub.dll"
done
user_imports=(GetStdHandle:4 WriteFile:20 GetLastError:0 ExitProcess:4
  GetCommandLineA:0 GetModuleFileNameA:12 GetSystemDirectoryA:8
  CopyFileA:12 DeleteFileA:4 Sleep:4 GetModuleHandleA:4 GetVersionExA:4
  CloseHandle:4 CreateFileA:28 DeviceIoControl:32 GetCurrentProcess:0
  GetProcAddress:8 GetProcessHeap:0 HeapAlloc:12 HeapFree:12 OpenProcess:12
  ReadProcessMemory:20 WriteProcessMemory:20 CreateEventA:16 ResetEvent:4
  SetEvent:4 WaitForSingleObject:8 SetLastError:4 lstrcmpiA:8 lstrlenA:4
  DialogBoxIndirectParamA:20 EndDialog:8 SetDlgItemTextA:12 GetDlgItem:8 EnableWindow:8 MessageBoxA:16
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
"$LD" /nologo /machine:x86 /subsystem:windows,3.51 /osversion:3.51 \
  /entry:Entry /nodefaultlib /dynamicbase:no /nxcompat:no /safeseh:no \
  "/out:$ROOT/DIST/NTPWR.EXE" "$B/ntpwr_cli.obj" \
  "$B/ntpwr_remap.obj" "$B/ntpwr_service.obj" "$B/runtime.obj" \
  "$B/KERNEL32.stub.lib" "$B/ADVAPI32.stub.lib" "$B/USER32.stub.lib" "${user_aliases[@]}"
python3 - "$ROOT/DIST/NTPWR.EXE" <<'PYTHON'
from pathlib import Path
import struct,sys
p=Path(sys.argv[1]); b=p.read_bytes()
pe=struct.unpack_from('<I',b,0x3c)[0]
assert b[:2]==b'MZ' and b[pe:pe+4]==b'PE\0\0'
assert struct.unpack_from('<H',b,pe+4)[0]==0x14c
op=pe+24
sub=struct.unpack_from('<H',b,op+68)[0]
assert sub==2, f'expected Windows GUI subsystem, got {sub}'
assert struct.unpack_from('<HH',b,op+48)==(3,51)
print(f'PASS {p.name}: PE32 i386, GUI subsystem, NT 3.51')
PYTHON
