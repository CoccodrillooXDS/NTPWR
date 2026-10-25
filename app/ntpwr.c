/* NTPWR installer and Winlogon-aware ACPI shutdown controller. */
#include "../driver/ntpwr_hook.h"
#include "../include/ntpwr_version.h"
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef long LONG;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HWND;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef void *LPVOID;
typedef DWORD *LPDWORD;
typedef HANDLE SC_HANDLE;
typedef LONG LPARAM;
typedef DWORD WPARAM;
typedef LONG (__stdcall *DLGPROC)(HWND, UINT, WPARAM, LPARAM);

#define WINAPI __stdcall
#define IMPORT __declspec(dllimport)
#define NULL ((void *)0)
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define SERVICE_KERNEL_DRIVER 1UL
#define SERVICE_DEMAND_START 3UL
#define SERVICE_AUTO_START 2UL
#define SERVICE_WIN32_OWN_PROCESS 0x10UL
#define SERVICE_CHANGE_CONFIG 2UL
#define KEY_SET_VALUE 2UL
#define REG_OPTION_NON_VOLATILE 0UL
#define SERVICE_ALL_ACCESS 0xF01FFUL
#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 1UL
#define FILE_SHARE_WRITE 2UL
#define OPEN_EXISTING 3UL
#define CREATE_ALWAYS 2UL
#define FILE_ATTRIBUTE_NORMAL 0x80UL
#define FILE_ATTRIBUTE_READONLY 1UL
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFUL
#define SERVICE_ERROR_NORMAL 1UL
#define SC_MANAGER_CREATE_SERVICE 2UL
#define SC_MANAGER_CONNECT 1UL
#define SERVICE_QUERY_STATUS 4UL
#define SERVICE_START 0x10UL
#define SERVICE_STOP 0x20UL
#define DELETE_ACCESS 0x10000UL
#define SERVICE_CONTROL_STOP 1UL
#define SERVICE_ACCEPT_STOP 1UL
#define SERVICE_START_PENDING 2UL
#define SERVICE_RUNNING 4UL
#define SERVICE_STOPPED 1UL
#define SERVICE_STOP_PENDING 3UL
#define ERROR_SERVICE_DOES_NOT_EXIST 1060UL
#define ERROR_SERVICE_ALREADY_RUNNING 1056UL
#define ERROR_SERVICE_CANNOT_ACCEPT_CTRL 1061UL
#define ERROR_SERVICE_NOT_ACTIVE 1062UL
#define HKEY_LOCAL_MACHINE ((HANDLE)0x80000002UL)
#define KEY_QUERY_VALUE 1UL
#define REG_DWORD 4UL
#define WM_INITDIALOG 0x0110UL
#define WM_COMMAND 0x0111UL
#define WM_CLOSE 0x0010UL
#define IDOK 1
#define IDCANCEL 2
#define WS_POPUP 0x80000000UL
#define WS_CAPTION 0x00C00000UL
#define WS_SYSMENU 0x00080000UL
#define WS_CHILD 0x40000000UL
#define WS_VISIBLE 0x10000000UL
#define WS_TABSTOP 0x00010000UL
#define DS_MODALFRAME 0x80UL
#define BS_PUSHBUTTON 0UL
#define BS_DEFPUSHBUTTON 1UL
#define BS_GROUPBOX 7UL
#define SS_LEFT 0UL
#define MB_OK 0UL
#define MB_ICONERROR 0x10UL
#define MB_ICONINFORMATION 0x40UL
#define CONTROLS 4096

#define IDC_DRIVER 101
#define IDC_ACPI 102
#define IDC_APM 103
#define IDC_METHOD 104
#define IDC_POWER 105
#define IDC_INSTALL 201
#define IDC_REMOVE 202
#define IDC_INFO 203
#define IDC_REFRESH 204
#define IDC_OS 301
#define IDC_INIT 302
#define IDC_LOAD 303
#define IDC_S5 304
#define IDC_APMDETAIL 305
#define IDC_BACKEND 306
#define IDC_HOOK 307
#define IDC_MODE 308
#define IDC_NAMESPACE 309

struct SERVICE_STATUS {
    DWORD dwServiceType, dwCurrentState, dwControlsAccepted;
    DWORD dwWin32ExitCode, dwServiceSpecificExitCode, dwCheckPoint, dwWaitHint;
};
struct OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber;
    DWORD dwPlatformId;
    char szCSDVersion[128];
};

typedef struct PROBE {
    BOOL installed, running, scanned;
    DWORD stage, init, load, ns, eval, count, a, b;
    DWORD error;
    DWORD backend;
    BOOL hook_active,hook_checked,hook_verified;
    DWORD hook_result;
    DWORD kernel_timestamp,kernel_checksum,kernel_image_size;
    DWORD branch_matches,branch_address,iat_address,hal_target,hal_import;
} PROBE;

IMPORT HANDLE WINAPI GetStdHandle(DWORD);
IMPORT BOOL WINAPI WriteFile(HANDLE, LPCSTR, DWORD, LPDWORD, LPVOID);
IMPORT BOOL WINAPI FreeConsole(void);
IMPORT DWORD WINAPI GetLastError(void);
IMPORT void WINAPI ExitProcess(DWORD);
IMPORT LPSTR WINAPI GetCommandLineA(void);
IMPORT DWORD WINAPI GetModuleFileNameA(HANDLE, LPSTR, DWORD);
IMPORT DWORD WINAPI GetSystemDirectoryA(LPSTR, DWORD);
IMPORT BOOL WINAPI CopyFileA(LPCSTR, LPCSTR, BOOL);
IMPORT BOOL WINAPI DeleteFileA(LPCSTR);
IMPORT DWORD WINAPI GetFileAttributesA(LPCSTR);
IMPORT BOOL WINAPI SetFileAttributesA(LPCSTR,DWORD);
IMPORT void WINAPI Sleep(DWORD);
IMPORT HANDLE WINAPI GetModuleHandleA(LPCSTR);
IMPORT BOOL WINAPI GetVersionExA(struct OSVERSIONINFOA *);
IMPORT DWORD WINAPI RegOpenKeyExA(HANDLE, LPCSTR, DWORD, DWORD, HANDLE *);
IMPORT DWORD WINAPI RegQueryValueExA(HANDLE, LPCSTR, LPDWORD, LPDWORD, BYTE *, LPDWORD);
IMPORT DWORD WINAPI RegCloseKey(HANDLE);
IMPORT SC_HANDLE WINAPI OpenSCManagerA(LPCSTR, LPCSTR, DWORD);
IMPORT SC_HANDLE WINAPI CreateServiceA(SC_HANDLE, LPCSTR, LPCSTR, DWORD, DWORD, DWORD,
    DWORD, LPCSTR, LPCSTR, LPDWORD, LPCSTR, LPCSTR, LPCSTR);
IMPORT SC_HANDLE WINAPI OpenServiceA(SC_HANDLE, LPCSTR, DWORD);
IMPORT BOOL WINAPI StartServiceA(SC_HANDLE, DWORD, LPCSTR *);
IMPORT BOOL WINAPI QueryServiceStatus(SC_HANDLE, struct SERVICE_STATUS *);
IMPORT BOOL WINAPI ControlService(SC_HANDLE, DWORD, struct SERVICE_STATUS *);
IMPORT BOOL WINAPI DeleteService(SC_HANDLE);
IMPORT BOOL WINAPI CloseServiceHandle(SC_HANDLE);
IMPORT BOOL WINAPI ChangeServiceConfigA(SC_HANDLE,DWORD,DWORD,DWORD,LPCSTR,LPCSTR,LPDWORD,LPCSTR,LPCSTR,LPCSTR,LPCSTR);
IMPORT DWORD WINAPI RegCreateKeyExA(HANDLE,LPCSTR,DWORD,LPSTR,DWORD,DWORD,LPVOID,HANDLE *,LPDWORD);
IMPORT DWORD WINAPI RegSetValueExA(HANDLE,LPCSTR,DWORD,DWORD,const BYTE *,DWORD);
IMPORT HANDLE WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPVOID,DWORD,DWORD,HANDLE);
IMPORT BOOL WINAPI DeviceIoControl(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPVOID);
IMPORT BOOL WINAPI CloseHandle(HANDLE);
extern BOOL NtpwrRunService(void);
extern BOOL NtpwrRemapAction(LPCSTR);

IMPORT LONG WINAPI DialogBoxIndirectParamA(HANDLE, const void *, HWND, DLGPROC, LPARAM);
IMPORT BOOL WINAPI EndDialog(HWND, LONG);
IMPORT BOOL WINAPI SetDlgItemTextA(HWND, int, LPCSTR);
IMPORT HWND WINAPI GetDlgItem(HWND, int);
IMPORT BOOL WINAPI EnableWindow(HWND, BOOL);
IMPORT int WINAPI MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);

static PROBE g_probe;
static DWORD g_main_storage[CONTROLS/4];
static DWORD g_info_storage[CONTROLS/4];
static BYTE *g_tpl;
static DWORD g_pos;
static HANDLE g_Output = (HANDLE)-1;
static HANDLE g_FallbackLog = (HANDLE)-1;
static BOOL g_CliMode = FALSE;
static char g_LastFailure[180];
static DWORD g_LastErrorCode;


static DWORD StringLength(LPCSTR s) { DWORD n=0; while(s[n]) ++n; return n; }
static BOOL WriteOutput(HANDLE handle,LPCSTR s,DWORD length) {
    DWORD written=0;
    return handle && handle!=(HANDLE)-1 &&
        WriteFile(handle,s,length,&written,NULL) && written==length;
}
static void Print(LPCSTR s) {
    DWORD length=StringLength(s);
    HANDLE error;
    DWORD written=0;
    if(!length || WriteOutput(g_Output,s,length))return;
    if(!g_CliMode)return;
    error=GetStdHandle(STD_ERROR_HANDLE);
    if(error!=g_Output && WriteOutput(error,s,length))return;
    if(g_FallbackLog==(HANDLE)-1)
        g_FallbackLog=CreateFileA("NTPWR-CLI.LOG",GENERIC_WRITE,
            FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(g_FallbackLog && g_FallbackLog!=(HANDLE)-1)
        WriteFile(g_FallbackLog,s,length,&written,NULL);
}
static void CliExit(DWORD status) {
    if(g_FallbackLog && g_FallbackLog!=(HANDLE)-1)
        CloseHandle(g_FallbackLog);
    ExitProcess(status);
}
static void SetFailure(LPCSTR reason,DWORD code) {
    DWORD i=0;
    while(reason[i] && i+1<sizeof(g_LastFailure)) {
        g_LastFailure[i]=reason[i];
        ++i;
    }
    g_LastFailure[i]=0;
    g_LastErrorCode=code;
}
static void Fail(LPCSTR reason) {
    SetFailure(reason,0);
    Print(reason);
    Print("\r\n");
}
static void HexToText(DWORD value, char *out) {
    static const char h[]="0123456789ABCDEF";
    int n;
    out[0]='0';out[1]='x';
    for(n=0;n<8;++n) out[n+2]=h[(value>>(28-n*4))&15UL];
    out[10]=0;
}
static void DecimalToText(DWORD value, char *out) {
    char rev[12]; int n=0,i;
    do { rev[n++]=(char)('0'+value%10UL); value/=10UL; } while(value);
    for(i=0;i<n;++i) out[i]=rev[n-i-1];
    out[n]=0;
}
static void PrintHex(DWORD x) { char s[11];HexToText(x,s);Print(s); }
static void PrintDecimal(DWORD x) { char s[12];DecimalToText(x,s);Print(s); }
static void Error(LPCSTR where) {
    DWORD code=GetLastError();
    SetFailure(where,code);
    Print(where);Print(" failed: ");PrintHex(code);Print("\r\n");
}
static void ShowFailure(HWND wnd,LPCSTR action) {
    char text[240],hex[11];
    DWORD i=0,j=0;
    LPCSTR cause=g_LastFailure[0]?g_LastFailure:"Operation failed";
    while(action[i] && i+1<sizeof(text)) { text[i]=action[i]; ++i; }
    text[i]=0;
    while(cause[j] && i+1<sizeof(text)) text[i++]=cause[j++];
    if(g_LastErrorCode && i+13<sizeof(text)) {
        HexToText(g_LastErrorCode,hex);
        text[i++]=' ';text[i++]='(';
        for(j=0;hex[j];++j) text[i++]=hex[j];
        text[i++]=')';
    }
    text[i]=0;
    MessageBoxA(wnd,text,"NTPWR",MB_OK|MB_ICONERROR);
}
static void ResetFailure(void) {
    g_LastFailure[0]=0;
    g_LastErrorCode=0;
}
static BOOL Equals(LPCSTR a, LPCSTR b) {
    char x,y;
    while(*a && *b) {
        x=*a++;y=*b++;
        if(x>='a'&&x<='z')x=(char)(x-32);
        if(y>='a'&&y<='z')y=(char)(y-32);
        if(x!=y)return FALSE;
    }
    return *a==0 && *b==0;
}
static BOOL Append(char *dst,DWORD max,LPCSTR tail) {
    DWORD i=StringLength(dst),j=0;
    while(tail[j]){if(i+1>=max)return FALSE;dst[i++]=tail[j++];}
    dst[i]=0;return TRUE;
}
static LPSTR Argument(void) {
    LPSTR s=GetCommandLineA();
    if(*s=='"') {
        ++s;
        while(*s && *s!='"') ++s;
        if(*s) ++s;
    } else {
        while(*s && *s!=' ' && *s!='\t') ++s;
    }
    while(*s==' ' || *s=='\t') ++s;
    return s;
}
static BOOL Paths(char *src,char *dst) {
    DWORD n=GetModuleFileNameA(NULL,src,MAX_PATH),i;
    if(!n||n>=MAX_PATH)return FALSE;
    i=n;while(i&&src[i-1]!='\\'&&src[i-1]!='/')--i;
    src[i]=0;
    if(!Append(src,MAX_PATH,"NTPWR.SYS"))return FALSE;
    n=GetSystemDirectoryA(dst,MAX_PATH);
    return n>0&&n<MAX_PATH&&Append(dst,MAX_PATH,"\\drivers\\NTPWR.SYS");
}
static SC_HANDLE Manager(void) {
    SC_HANDLE m=OpenSCManagerA(NULL,NULL,SC_MANAGER_CONNECT|SC_MANAGER_CREATE_SERVICE);
    if(!m)Error("OpenSCManager");
    return m;
}
static SC_HANDLE Service(SC_HANDLE m) {
    return OpenServiceA(m,"NTPWR",SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE_ACCESS);
}
static BOOL ServiceState(SC_HANDLE m,BOOL *installed,BOOL *running) {
    SC_HANDLE s=Service(m);struct SERVICE_STATUS st;
    *installed=FALSE;*running=FALSE;
    if(!s){if(GetLastError()==ERROR_SERVICE_DOES_NOT_EXIST)return TRUE; Error("OpenService");return FALSE;}
    *installed=TRUE;
    if(!QueryServiceStatus(s,&st)){Error("QueryServiceStatus");CloseServiceHandle(s);return FALSE;}
    *running=st.dwCurrentState==SERVICE_RUNNING;
    CloseServiceHandle(s);return TRUE;
}
static DWORD WritableAttributes(DWORD attr) {
    attr&=~FILE_ATTRIBUTE_READONLY;
    return attr?attr:FILE_ATTRIBUTE_NORMAL;
}
static BOOL CopyWritable(LPCSTR source,LPCSTR dest) {
    DWORD attr=GetFileAttributesA(dest);
    if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_READONLY) &&
       !SetFileAttributesA(dest,WritableAttributes(attr))){
        Error("SetFileAttributes");return FALSE;
    }
    if(!CopyFileA(source,dest,FALSE)){Error("CopyFile");return FALSE;}
    attr=GetFileAttributesA(dest);
    if(attr==INVALID_FILE_ATTRIBUTES){Error("GetFileAttributes");return FALSE;}
    if((attr&FILE_ATTRIBUTE_READONLY) &&
       !SetFileAttributesA(dest,WritableAttributes(attr))){
        Error("SetFileAttributes");return FALSE;
    }
    return TRUE;
}
static BOOL DeleteWritable(LPCSTR path) {
    DWORD attr=GetFileAttributesA(path);
    if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_READONLY) &&
       !SetFileAttributesA(path,WritableAttributes(attr)))return FALSE;
    return DeleteFileA(path);
}
static LPCSTR HookStatus(DWORD result) {
    switch(result){
    case NTPWR_OK:return "Compatible";
    case NTPWR_BAD_KERNEL_BASE:
    case NTPWR_BAD_PE:
    case NTPWR_BAD_IMAGE:return "Unsupported kernel image";
    case NTPWR_NO_TEXT:
    case NTPWR_NO_BRANCH:return "Shutdown path not found";
    case NTPWR_AMBIGUOUS_BRANCH:return "Ambiguous shutdown path";
    case NTPWR_BAD_IAT:
    case NTPWR_BAD_HAL_TARGET:return "HAL import not verified";
    case NTPWR_NOT_VERIFIED:return "HAL target not verified";
    case NTPWR_ALREADY_MODIFIED:return "HAL import already modified";
    case NTPWR_NO_ACPI_SHUTDOWN:return "ACPI S5 unavailable";
    default:return "Not supported";
    }
}
static LPCSTR AcpiState(const PROBE *p) {
    if(!p->scanned)return "Not scanned";
    if(p->stage==1 && p->init==6UL)return "Not detected (no ACPI tables)";
    if(p->stage!=4)return "Initialization failed";
    return p->backend?"Ready (S5)":"Detected; S5 unavailable";
}
static BOOL Install(SC_HANDLE m) {
    char src[MAX_PATH],dst[MAX_PATH];SC_HANDLE existing,s;
    existing=Service(m);
    if(existing){CloseServiceHandle(existing);Fail("NTPWR already installed. Remove the previous version first.");return FALSE;}
    if(GetLastError()!=ERROR_SERVICE_DOES_NOT_EXIST){Error("OpenService");return FALSE;}
    if(!Paths(src,dst)){Fail("Driver path unavailable");return FALSE;}
    if(!CopyWritable(src,dst))return FALSE;
    s=CreateServiceA(m,"NTPWR","NT Power Management Driver",
        SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE_ACCESS,
        SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
        "\\SystemRoot\\System32\\Drivers\\NTPWR.SYS",NULL,NULL,NULL,NULL,NULL);
    if(!s){Error("CreateService");DeleteWritable(dst);return FALSE;}
    CloseServiceHandle(s);Print("Driver installed.\r\n");return TRUE;
}
static BOOL Start(SC_HANDLE m) {
    SC_HANDLE s=Service(m);BOOL ok=FALSE;
    if(!s){Error("OpenService");return FALSE;}
    if(StartServiceA(s,0,NULL)){Print("Driver loaded.\r\n");ok=TRUE;}
    else if(GetLastError()==ERROR_SERVICE_ALREADY_RUNNING){ok=TRUE;}
    else Error("StartService");
    CloseServiceHandle(s);return ok;
}
static BOOL WaitStopped(SC_HANDLE s) {
    struct SERVICE_STATUS st;unsigned i;
    for(i=0;i<30;++i){
        if(!QueryServiceStatus(s,&st))return FALSE;
        if(st.dwCurrentState==SERVICE_STOPPED)return TRUE;
        if(st.dwCurrentState!=SERVICE_STOP_PENDING)return FALSE;
        Sleep(100);
    }
    return FALSE;
}
static BOOL Stop(SC_HANDLE m) {
    SC_HANDLE s=Service(m);struct SERVICE_STATUS st;BOOL ok=FALSE;
    if(!s){Error("OpenService");return FALSE;}
    if(!QueryServiceStatus(s,&st)){Error("QueryServiceStatus");goto done;}
    if(st.dwCurrentState==SERVICE_STOPPED){ok=TRUE;goto done;}
    if(st.dwCurrentState!=SERVICE_STOP_PENDING && !ControlService(s,SERVICE_CONTROL_STOP,&st)){
        if(GetLastError()!=ERROR_SERVICE_NOT_ACTIVE){Error("ControlService");goto done;}
    }
    if(!WaitStopped(s)){Fail("Driver did not stop");goto done;}
    ok=TRUE;
done:
    CloseServiceHandle(s);
    if(ok)Print("Driver stopped.\r\n");
    return ok;
}
static BOOL Remove(SC_HANDLE m) {
    SC_HANDLE s=Service(m);char src[MAX_PATH],dst[MAX_PATH];
    struct SERVICE_STATUS st;BOOL ok=FALSE;
    if(!s){Error("OpenService");return FALSE;}
    if(!QueryServiceStatus(s,&st)){Error("QueryServiceStatus");goto done;}
    if(st.dwCurrentState!=SERVICE_STOPPED){
        if(st.dwCurrentState!=SERVICE_STOP_PENDING && !ControlService(s,SERVICE_CONTROL_STOP,&st)){
            Error("ControlService");goto done;
        }
        if(!WaitStopped(s)){Fail("Driver did not stop");goto done;}
        Print("Driver stopped.\r\n");
    }
    if(!DeleteService(s)){Error("DeleteService");goto done;}
    ok=TRUE;Print("Driver service removed.\r\n");
done:
    CloseServiceHandle(s);
    if(ok&&Paths(src,dst)){
        if(!DeleteWritable(dst)){
            Print("Warning: unable to delete driver file: ");PrintHex(GetLastError());Print("\r\n");
        }else Print("Driver file removed.\r\n");
    }
    return ok;
}
static BOOL ReadDword(HANDLE key,LPCSTR name,DWORD *v) {
    DWORD type=0,n=sizeof(*v);
    return RegQueryValueExA(key,name,NULL,&type,(BYTE *)v,&n)==0 &&
        type==REG_DWORD && n==sizeof(*v);
}
static BOOL ReadProbe(PROBE *p) {
    HANDLE key;
    DWORD rc=RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\NTPWR",0,KEY_QUERY_VALUE,&key);
    if(rc){p->error=rc;return FALSE;}
    p->scanned=ReadDword(key,"ProbeStage",&p->stage)&&
        ReadDword(key,"InitStatus",&p->init)&&
        ReadDword(key,"LoadStatus",&p->load)&&
        ReadDword(key,"NamespaceStatus",&p->ns)&&
        ReadDword(key,"EvalStatus",&p->eval)&&
        ReadDword(key,"S5Count",&p->count)&&
        ReadDword(key,"S5TypeA",&p->a)&&
        ReadDword(key,"S5TypeB",&p->b);
    RegCloseKey(key);return p->scanned;
}
static BOOL KernelHook(DWORD action,NTPWR_REPORT *out) {
    HANDLE d=CreateFileA("\\\\.\\NTPWR",GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    DWORD count=0;
    BOOL ok;
    if(!d || d==(HANDLE)-1)return FALSE;
    ok=DeviceIoControl(d,action,NULL,0,out,
        out?(DWORD)sizeof(*out):0,&count,NULL);
    if(ok&&out&&count!=sizeof(*out))ok=FALSE;
    CloseHandle(d);
    return ok;
}
static BOOL QueryHook(PROBE *p) {
    NTPWR_REPORT report;
    if(!KernelHook(NTPWR_IOCTL_QUERY,&report))return FALSE;
    if(report.Signature!=NTPWR_SIGNATURE || report.Size!=sizeof(report))return FALSE;
    p->backend=report.AcpiReady;
    p->hook_active=(report.Flags & NTPWR_FLAG_ARMED)!=0;
    p->hook_result=report.Result;
    p->hook_verified=(report.Flags&NTPWR_FLAG_KNOWN)!=0;
    p->kernel_timestamp=report.KernelTimestamp;
    p->kernel_checksum=report.KernelChecksum;
    p->kernel_image_size=report.KernelImageSize;
    p->branch_matches=report.BranchMatches;
    p->branch_address=report.BranchAddress;
    p->iat_address=report.IatAddress;
    p->hal_target=report.HalTarget;
    p->hal_import=report.HalImportFromTable;
    p->hook_checked=TRUE;
    return TRUE;
}
static BOOL HookArm(void) {
    NTPWR_REPORT report;
    if(KernelHook(NTPWR_IOCTL_QUERY,&report) &&
       report.Signature==NTPWR_SIGNATURE && report.Size==sizeof(report)){
        if(!report.AcpiReady){
            PROBE probe;
            if(ReadProbe(&probe) && probe.stage==1 && probe.init==6UL)
                Fail("ACPI tables not found; APM not implemented");
            else
                Fail("ACPI S5 unavailable; APM not implemented");
            if(report.Result!=NTPWR_OK){
                Print("NTPWR: ");Print(HookStatus(report.Result));Print("\r\n");
            }
            return FALSE;
        }
        if(report.Result!=NTPWR_OK){
            Fail(HookStatus(report.Result));
            return FALSE;
        }
    }
    if(!KernelHook(NTPWR_IOCTL_ARM,NULL)){
        Error("NTPWR hook arm");return FALSE;
    }
    return TRUE;
}
static BOOL HookDisarm(void) {
    if(!KernelHook(NTPWR_IOCTL_DISARM,NULL)){
        Error("NTPWR hook disarm");return FALSE;
    }
    return TRUE;
}
static BOOL WaitAgentState(SC_HANDLE service, DWORD expected) {
    struct SERVICE_STATUS status;
    unsigned i;
    for(i=0;i<60;++i){
        if(!QueryServiceStatus(service,&status))return FALSE;
        if(status.dwCurrentState==expected)return TRUE;
        if(status.dwCurrentState==SERVICE_STOPPED && expected==SERVICE_RUNNING)
            return FALSE;
        Sleep(500);
    }
    return FALSE;
}
static BOOL StopAgent(SC_HANDLE service) {
    struct SERVICE_STATUS status;
    unsigned i;
    for(i=0;i<60;++i){
        if(!QueryServiceStatus(service,&status)){
            Error("QueryServiceStatus(NTPWRAgent)");return FALSE;
        }
        if(status.dwCurrentState==SERVICE_STOPPED)return TRUE;
        if(status.dwCurrentState==SERVICE_START_PENDING ||
           status.dwCurrentState==SERVICE_STOP_PENDING){
            Sleep(500);continue;
        }
        if(!(status.dwControlsAccepted&SERVICE_ACCEPT_STOP)){
            Fail("NTPWRAgent does not accept stop");return FALSE;
        }
        if(!ControlService(service,SERVICE_CONTROL_STOP,&status)){
            DWORD error=GetLastError();
            if(error!=ERROR_SERVICE_CANNOT_ACCEPT_CTRL &&
               error!=ERROR_SERVICE_NOT_ACTIVE){
                Error("Stop(NTPWRAgent)");return FALSE;
            }
        }
        Sleep(500);
    }
    Fail("NTPWRAgent stop timed out");
    return FALSE;
}
static BOOL AgentInstall(SC_HANDLE manager, BOOL *rollbackSafe) {
    char path[MAX_PATH],image[MAX_PATH+32],src[MAX_PATH];
    SC_HANDLE service,existing;
    DWORD n=GetSystemDirectoryA(path,MAX_PATH);
    DWORD len=GetModuleFileNameA(NULL,src,MAX_PATH);
    *rollbackSafe=TRUE;
    existing=OpenServiceA(manager,"NTPWRAgent",SERVICE_QUERY_STATUS);
    if(existing){
        CloseServiceHandle(existing);
        Fail("NTPWRAgent already installed");
        return FALSE;
    }
    if(GetLastError()!=ERROR_SERVICE_DOES_NOT_EXIST){
        Error("OpenService(NTPWRAgent)");return FALSE;
    }
    if(!n || n>=MAX_PATH || !len || len>=MAX_PATH)return FALSE;
    if(!Append(path,MAX_PATH,"\\NTPWR.EXE"))return FALSE;
    if(!CopyWritable(src,path))return FALSE;
    image[0]='"';image[1]=0;
    if(!Append(image,sizeof(image),path) ||
       !Append(image,sizeof(image),"\" --service")){
        DeleteWritable(path);return FALSE;
    }
    service=CreateServiceA(manager,"NTPWRAgent","NT Power Management Agent",
        SERVICE_ALL_ACCESS,SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,image,NULL,NULL,NULL,NULL,NULL);
    if(!service){
        Error("CreateService(NTPWRAgent)");
        DeleteWritable(path);return FALSE;
    }
    if(!StartServiceA(service,0,NULL)){
        Error("StartService(NTPWRAgent)");
        goto rollback;
    }
    if(!WaitAgentState(service,SERVICE_RUNNING)){
        Fail("NTPWRAgent failed to start");
        goto rollback;
    }
    CloseServiceHandle(service);
    return TRUE;
rollback:
    if(!StopAgent(service) || !NtpwrRemapAction("RESTORE")){
        *rollbackSafe=FALSE;
        Fail("NTPWRAgent rollback incomplete");
    }else if(!DeleteService(service)){
        *rollbackSafe=FALSE;
        Error("DeleteService(NTPWRAgent)");
    }
    CloseServiceHandle(service);
    if(*rollbackSafe && !DeleteWritable(path))
        Print("Warning: NTPWR.EXE cleanup pending.\r\n");
    return FALSE;
}
static BOOL AgentRemove(SC_HANDLE manager) {
    SC_HANDLE service=OpenServiceA(manager,"NTPWRAgent",SERVICE_STOP|
                                  SERVICE_QUERY_STATUS|DELETE_ACCESS);
    if(!service)return GetLastError()==ERROR_SERVICE_DOES_NOT_EXIST;
    if(!StopAgent(service)){
        CloseServiceHandle(service);return FALSE;
    }
    if(!DeleteService(service)){
        Error("DeleteService(NTPWRAgent)");CloseServiceHandle(service);return FALSE;
    }
    CloseServiceHandle(service);return TRUE;
}
static BOOL EnableAutostart(SC_HANDLE manager) {
    HANDLE key=NULL;SC_HANDLE service;DWORD value=1,err;
    err=RegCreateKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\NTPWR",
        0,NULL,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,NULL,&key,NULL);
    if(err!=0)return FALSE;
    err=RegSetValueExA(key,"AutoArm",0,REG_DWORD,(const BYTE *)&value,sizeof(value));
    RegCloseKey(key);
    if(err!=0)return FALSE;
    service=OpenServiceA(manager,"NTPWR",SERVICE_CHANGE_CONFIG);
    if(!service)return FALSE;
    value=ChangeServiceConfigA(service,0xFFFFFFFFUL,SERVICE_AUTO_START,
        0xFFFFFFFFUL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
    CloseServiceHandle(service);return value!=0;
}
static BOOL InstallFull(SC_HANDLE manager) {
    SC_HANDLE conflict=OpenServiceA(manager,"QEMUPWR",SERVICE_QUERY_STATUS);
    BOOL ok,agentRollbackSafe=TRUE;
    if(conflict){CloseServiceHandle(conflict);Fail("Remove QEMUPWR before installing NTPWR");return FALSE;}
    if(!NtpwrRemapAction("VERIFY")){
        Fail("Winlogon shutdown path not verified");
        return FALSE;
    }
    if(!Install(manager))return FALSE;
    if(!Start(manager))goto rollback;
    if(!HookArm())goto rollback;
    if(!AgentInstall(manager,&agentRollbackSafe)){
        if(!agentRollbackSafe || !HookDisarm())return FALSE;
        goto rollback;
    }
    if(!EnableAutostart(manager)){
        Fail("Failed to enable automatic startup");
        if(!AgentRemove(manager) || !HookDisarm())return FALSE;
        goto rollback;
    }
    return TRUE;
rollback:
    ok=Remove(manager);
    (void)ok;
    return FALSE;
}
static BOOL RemoveFull(SC_HANDLE manager) {
    BOOL found=FALSE,running=FALSE;
    if(!AgentRemove(manager)){
        if(!g_LastFailure[0]) Fail("Failed to remove NTPWRAgent");
        return FALSE;
    }
    if(!ServiceState(manager,&found,&running))return FALSE;
    if(!found){Print("NTPWR: not installed.\r\n");return TRUE;}
    if(running && !HookDisarm())return FALSE;
    if(!Remove(manager))return FALSE;
    {
        char dest[MAX_PATH];DWORD n=GetSystemDirectoryA(dest,MAX_PATH);
        if(n && n<MAX_PATH && Append(dest,MAX_PATH,"\\NTPWR.EXE"))
            if(!DeleteWritable(dest))Print("Warning: NTPWR.EXE cleanup pending.\r\n");
    }
    return TRUE;
}
static BOOL Scan(SC_HANDLE m,PROBE *p) {
    BOOL was_installed=FALSE,was_running=FALSE;
    BOOL temporary=FALSE,started=FALSE,ok=FALSE;
    p->scanned=FALSE;p->error=0;p->backend=0;p->hook_active=FALSE;
    p->hook_checked=FALSE;p->hook_verified=FALSE;p->hook_result=0;
    if(!ServiceState(m,&was_installed,&was_running))return FALSE;
    p->installed=was_installed;p->running=was_running;
    if(!was_installed){
        if(!Install(m))return FALSE;
        temporary=TRUE;
    }
    if(!was_running){
        if(!Start(m))goto cleanup;
        started=TRUE;
    }
    ok=ReadProbe(p);
    if(ok)QueryHook(p);
    if(!ok)Print("Probe results unavailable.\r\n");
cleanup:
    if(temporary){
        if(!Remove(m))ok=FALSE;
    }else if(started){
        if(!Stop(m))ok=FALSE;
    }
    p->installed=was_installed;p->running=was_running;
    return ok;
}
static void PrintProbe(const PROBE *p) {
    Print("NTPWR: ACPI ");
    if(!p->scanned)Print("not scanned\r\n");
    else if(p->stage==1 && p->init==6UL)Print("not detected (ACPI tables unavailable)\r\n");
    else if(p->stage==4){
        Print("detected; AML ready; S5 package=");PrintDecimal(p->count);
        Print(" slp_typa=");PrintDecimal(p->a);
        Print(" slp_typb=");PrintDecimal(p->b);Print("\r\n");
    }else{
        Print("probe incomplete; stage=");PrintDecimal(p->stage);
        Print(" init=");PrintHex(p->init);
        Print(" load=");PrintHex(p->load);
        Print(" namespace=");PrintHex(p->ns);
        Print(" eval=");PrintHex(p->eval);Print("\r\n");
    }
    Print("NTPWR: APM not implemented (not probed)\r\n");
    Print("NTPWR: ACPI S5 ");Print(p->backend?"registers mapped\r\n":"not ready\r\n");
    if(p->hook_checked){
        Print("NTPWR: Hook: ");
        Print(p->hook_result==NTPWR_OK&&!p->hook_verified?
              "Compatible (unverified)":HookStatus(p->hook_result));
        Print("\r\n");
        if(p->hook_result==NTPWR_NOT_VERIFIED){
            Print("NTPWR: Kernel timestamp=");PrintHex(p->kernel_timestamp);
            Print(" checksum=");PrintHex(p->kernel_checksum);
            Print(" size=");PrintHex(p->kernel_image_size);Print("\r\n");
            Print("NTPWR: Shutdown branch=");PrintHex(p->branch_address);
            Print(" matches=");PrintDecimal(p->branch_matches);
            Print(" IAT=");PrintHex(p->iat_address);Print("\r\n");
            Print("NTPWR: HAL target=");PrintHex(p->hal_target);
            Print(" import=");PrintHex(p->hal_import);Print("\r\n");
        }
    }
}
static BOOL Run(LPCSTR action) {
    SC_HANDLE m=Manager();BOOL ok=FALSE;
    if(!m)return FALSE;
    if(Equals(action,"INSTALL")) {ok=InstallFull(m);}
    else if(Equals(action,"START"))ok=Start(m);
    else if(Equals(action,"STOP"))ok=Stop(m);
    else if(Equals(action,"REMOVE"))ok=RemoveFull(m);
    else if(Equals(action,"SCAN")){
        ok=Scan(m,&g_probe);PrintProbe(&g_probe);
    }else if(Equals(action,"INFO")||Equals(action,"RESULT")){
        BOOL found=FALSE,running=FALSE;
        if(ServiceState(m,&found,&running)&&found&&running){
            g_probe.installed=found;g_probe.running=running;
            ok=ReadProbe(&g_probe);
            if(ok)QueryHook(&g_probe);
            PrintProbe(&g_probe);
        }else{
            ok=Scan(m,&g_probe);PrintProbe(&g_probe);
        }
    }else if(Equals(action,"STATUS")){
        BOOL found=FALSE,running=FALSE;
        ok=ServiceState(m,&found,&running);
        if(ok){Print("NTPWR: ");Print(!found?"Not installed\r\n":running?"Running\r\n":"Stopped\r\n");}
    }
    CloseServiceHandle(m);return ok;
}

static void W(WORD x){g_tpl[g_pos++]=(BYTE)x;g_tpl[g_pos++]=(BYTE)(x>>8);}
static void D(DWORD x){W((WORD)x);W((WORD)(x>>16));}
static void T(LPCSTR s){while(*s)W((WORD)(BYTE)*s++);W(0);}
static void Align4(void){while(g_pos&3UL)g_tpl[g_pos++]=0;}
static void Control(DWORD style,WORD x,WORD y,WORD width,WORD height,WORD id,WORD cls,LPCSTR text){
    Align4();D(style|WS_CHILD|WS_VISIBLE);D(0);W(x);W(y);W(width);W(height);
    W(id);W(0xFFFF);W(cls);T(text);W(0);
}
static void Begin(BYTE *buf,WORD count,WORD width,WORD height,LPCSTR name){
    g_tpl=buf;g_pos=0;D(WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME);
    D(0);W(count);W(0);W(0);W(width);W(height);W(0);W(0);T(name);
}
static const void *MainTemplate(void){
    Begin((BYTE *)g_main_storage,15,274,164,
          "NT Power Management Driver - " NTPWR_VERSION_DISPLAY);
    Control(BS_GROUPBOX,8,7,258,120,20,0x80,"Status");
    Control(SS_LEFT,17,25,69,12,21,0x82,"Driver:");
    Control(SS_LEFT,91,25,165,12,IDC_DRIVER,0x82,"");
    Control(SS_LEFT,17,44,69,12,22,0x82,"ACPI:");
    Control(SS_LEFT,91,44,165,12,IDC_ACPI,0x82,"");
    Control(SS_LEFT,17,63,69,12,23,0x82,"APM:");
    Control(SS_LEFT,91,63,165,12,IDC_APM,0x82,"");
    Control(SS_LEFT,17,82,69,12,24,0x82,"Backend:");
    Control(SS_LEFT,91,82,165,12,IDC_METHOD,0x82,"");
    Control(SS_LEFT,17,101,69,12,25,0x82,"Power-off:");
    Control(SS_LEFT,91,101,165,12,IDC_POWER,0x82,"");
    Control(BS_PUSHBUTTON|WS_TABSTOP,8,138,58,17,IDC_INSTALL,0x80,"Install");
    Control(BS_PUSHBUTTON|WS_TABSTOP,74,138,65,17,IDC_REMOVE,0x80,"Uninstall");
    Control(BS_PUSHBUTTON|WS_TABSTOP,147,138,56,17,IDC_INFO,0x80,"Info...");
    Control(BS_DEFPUSHBUTTON|WS_TABSTOP,211,138,55,17,IDCANCEL,0x80,"Close");
    return g_main_storage;
}
static const void *InfoTemplate(void){
    Begin((BYTE *)g_info_storage,21,295,239,
          "NTPWR - Information - " NTPWR_VERSION_DISPLAY);
    Control(BS_GROUPBOX,8,7,279,196,30,0x80,"Diagnostics");
    Control(SS_LEFT,16,24,72,12,31,0x82,"Windows:");
    Control(SS_LEFT,96,24,177,12,IDC_OS,0x82,"");
    Control(SS_LEFT,16,43,72,12,32,0x82,"ACPI:");
    Control(SS_LEFT,96,43,177,12,IDC_ACPI,0x82,"");
    Control(SS_LEFT,16,62,72,12,33,0x82,"Init status:");
    Control(SS_LEFT,96,62,177,12,IDC_INIT,0x82,"");
    Control(SS_LEFT,16,81,72,12,34,0x82,"AML status:");
    Control(SS_LEFT,96,81,177,12,IDC_LOAD,0x82,"");
    Control(SS_LEFT,16,100,72,12,35,0x82,"S5 values:");
    Control(SS_LEFT,96,100,177,12,IDC_S5,0x82,"");
    Control(SS_LEFT,16,119,72,12,36,0x82,"APM:");
    Control(SS_LEFT,96,119,177,12,IDC_APMDETAIL,0x82,"");
    Control(SS_LEFT,16,138,72,12,37,0x82,"Backend:");
    Control(SS_LEFT,96,138,177,12,IDC_BACKEND,0x82,"");
    Control(SS_LEFT,16,157,72,12,38,0x82,"Shutdown hook:");
    Control(SS_LEFT,96,157,177,12,IDC_HOOK,0x82,"");
    Control(SS_LEFT,16,176,72,12,39,0x82,"Namespace:");
    Control(SS_LEFT,96,176,177,12,IDC_NAMESPACE,0x82,"");
    Control(BS_PUSHBUTTON|WS_TABSTOP,91,211,75,17,IDC_REFRESH,0x80,"Refresh");
    Control(BS_DEFPUSHBUTTON|WS_TABSTOP,197,211,88,17,IDCANCEL,0x80,"Close");
    return g_info_storage;
}
static void Text(HWND wnd,int id,LPCSTR val){SetDlgItemTextA(wnd,id,val);}
static void UpdateMain(HWND wnd){
    Text(wnd,IDC_DRIVER,g_probe.installed?(g_probe.running?"Running":"Installed (stopped)"):"Not installed");
    Text(wnd,IDC_ACPI,AcpiState(&g_probe));
    Text(wnd,IDC_APM,"Not implemented");
    Text(wnd,IDC_METHOD,g_probe.backend?"ACPI S5":"None");
    Text(wnd,IDC_POWER,g_probe.hook_active?"Enabled":g_probe.backend?"Disabled":"Unavailable");
    EnableWindow(GetDlgItem(wnd,IDC_INSTALL),!g_probe.installed);
    EnableWindow(GetDlgItem(wnd,IDC_REMOVE),g_probe.installed);
}
static void MakeVersion(char *text){
    struct OSVERSIONINFOA v;char major[12],minor[12],build[12];
    v.dwOSVersionInfoSize=sizeof(v);
    if(!GetVersionExA(&v)){text[0]=0;Append(text,100,"Unavailable");return;}
    DecimalToText(v.dwMajorVersion,major);DecimalToText(v.dwMinorVersion,minor);
    DecimalToText(v.dwBuildNumber&0xFFFFUL,build);
    text[0]=0;Append(text,100,"NT ");Append(text,100,major);Append(text,100,".");
    Append(text,100,minor);Append(text,100," (build ");Append(text,100,build);Append(text,100,")");
}
static void UpdateInfo(HWND wnd){
    char os[100],s[100],a[12],b[12],count[12];
    MakeVersion(os);Text(wnd,IDC_OS,os);
    Text(wnd,IDC_ACPI,AcpiState(&g_probe));
    if(!g_probe.scanned){
        Text(wnd,IDC_INIT,"Unavailable");Text(wnd,IDC_LOAD,"Unavailable");
        Text(wnd,IDC_S5,"Unavailable");
    }else{
        HexToText(g_probe.init,s);Text(wnd,IDC_INIT,s);
        HexToText(g_probe.load,s);Text(wnd,IDC_LOAD,s);
        if(g_probe.stage==4){
            DecimalToText(g_probe.count,count);DecimalToText(g_probe.a,a);DecimalToText(g_probe.b,b);
            s[0]=0;Append(s,100,"Package ");Append(s,100,count);Append(s,100,", types ");
            Append(s,100,a);Append(s,100," / ");Append(s,100,b);
        }else{HexToText(g_probe.eval,s);}
        Text(wnd,IDC_S5,s);
    }
    Text(wnd,IDC_APMDETAIL,"Not implemented");
    if(g_probe.scanned){HexToText(g_probe.ns,s);Text(wnd,IDC_NAMESPACE,s);}
    else Text(wnd,IDC_NAMESPACE,"Unavailable");
    Text(wnd,IDC_BACKEND,g_probe.backend?"ACPI S5 registers mapped":"No supported backend");
    Text(wnd,IDC_HOOK,!g_probe.hook_checked?"Not scanned":
         g_probe.hook_active?"Active":
         g_probe.hook_result==NTPWR_OK&&!g_probe.hook_verified?
         "Compatible (unverified)":HookStatus(g_probe.hook_result));
}
static BOOL Refresh(void){
    SC_HANDLE m=Manager();BOOL ok;
    if(!m)return FALSE;
    ok=Scan(m,&g_probe);
    CloseServiceHandle(m);
    return ok;
}
static LONG WINAPI InfoDialog(HWND wnd,UINT msg,WPARAM w,LPARAM l){
    (void)l;
    if(msg==WM_INITDIALOG){UpdateInfo(wnd);return TRUE;}
    if(msg==WM_CLOSE){EndDialog(wnd,0);return TRUE;}
    if(msg==WM_COMMAND){
        if((WORD)w==IDCANCEL){EndDialog(wnd,0);return TRUE;}
        if((WORD)w==IDC_REFRESH){Refresh();UpdateInfo(wnd);return TRUE;}
    }
    return FALSE;
}
static LONG WINAPI MainDialog(HWND wnd,UINT msg,WPARAM w,LPARAM l){
    (void)l;
    if(msg==WM_INITDIALOG){Refresh();UpdateMain(wnd);return TRUE;}
    if(msg==WM_CLOSE){EndDialog(wnd,0);return TRUE;}
    if(msg==WM_COMMAND){
        if((WORD)w==IDCANCEL){EndDialog(wnd,0);return TRUE;}
        if((WORD)w==IDC_INFO){
            DialogBoxIndirectParamA(GetModuleHandleA(NULL),InfoTemplate(),wnd,InfoDialog,0);
            return TRUE;
        }
        if((WORD)w==IDC_INSTALL){
            SC_HANDLE m;BOOL ok=FALSE;
            ResetFailure();
            m=Manager();
            if(m){ok=InstallFull(m);CloseServiceHandle(m);}
            if(!ok)ShowFailure(wnd,"Installation failed: ");
            Refresh();UpdateMain(wnd);return TRUE;
        }
        if((WORD)w==IDC_REMOVE){
            SC_HANDLE m;BOOL ok=FALSE;
            ResetFailure();
            m=Manager();
            if(m){ok=RemoveFull(m);CloseServiceHandle(m);}
            if(!ok)ShowFailure(wnd,"Uninstall failed: ");
            Refresh();UpdateMain(wnd);return TRUE;
        }
    }
    return FALSE;
}
void __cdecl Entry(void){
    LPSTR arg=Argument();BOOL ok;
    g_Output=GetStdHandle(STD_OUTPUT_HANDLE);
    g_CliMode=(*arg!=0 && !Equals(arg,"--service"));
    if(Equals(arg,"--service")){FreeConsole();ExitProcess(NtpwrRunService()?0:1);}
    if(!*arg){
        LONG r;
        FreeConsole();
        g_Output=(HANDLE)-1;
        r=DialogBoxIndirectParamA(GetModuleHandleA(NULL),MainTemplate(),NULL,MainDialog,0);
        if(r==-1){Error("DialogBoxIndirectParam");ExitProcess(1);}
        ExitProcess(0);
    }
    if(Equals(arg,"WINLOGON")){
        ok=NtpwrRemapAction("STATUS");
        CliExit(ok?0:1);
    }
    if(!Equals(arg,"INSTALL")&&!Equals(arg,"START")&&!Equals(arg,"STOP")&&
       !Equals(arg,"REMOVE")&&!Equals(arg,"STATUS")&&!Equals(arg,"SCAN")&&
       !Equals(arg,"INFO")&&!Equals(arg,"RESULT")){
        Print("Usage: NTPWR [SCAN|INFO|WINLOGON|INSTALL|START|STATUS|STOP|REMOVE]\r\n");
        CliExit(2);
    }
    ResetFailure();
    ok=Run(arg);
    CliExit(ok?0:1);
}
