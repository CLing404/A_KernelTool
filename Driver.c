#include <fltKernel.h>
#include "KernCmdShared.h"

typedef NTSTATUS(NTAPI* PFN_IO_CREATE_DEVICE_SECURE)(
    PDRIVER_OBJECT  DriverObject,
    ULONG           DeviceExtensionSize,
    PUNICODE_STRING DeviceName,
    ULONG           DeviceType,
    ULONG           DeviceCharacteristics,
    BOOLEAN         Exclusive,
    PUNICODE_STRING DefaultSDDLString,
    PVOID           DeviceClassGuid,
    PDEVICE_OBJECT* DeviceObject);

#ifndef offsetof
#define offsetof(s, m) ((SIZE_T)(ULONG_PTR)&(((s *)0)->m))
#endif

#ifndef FLTAPI
#define FLTAPI NTAPI
#endif

enum {
    KC_TAG = 0x646D434B,
    KC_PROCESS_TERMINATE = 0x00000001,
    KC_PROCESS_SET_INFORMATION = 0x00000200,
    KC_PROCESS_QUERY_INFORMATION = 0x00000400,
    KC_INFO_BREAK_ON_TERM = 29,
    KC_INFO_PROTECTION = 61,
    KC_KEY_READ = 0x00020019,
    KC_CLASS_DIR_INFORMATION = 1,
    KC_CLASS_KEY_VALUE_PARTIAL = 2,
    KC_FP_DISPOSITION_DELETE = 0x1,
    KC_COPY_CHUNK = 64 * 1024,
    KC_RD_MAX_NODES = 4096,
    KC_SCAN_BYTES = 64,
    KC_MAX_OFFSET_SCAN = 0x1000,
    KC_SAMPLE_MAX = 16,
    KC_FLT_MAX_VOLUMES = 16,
    KC_PORT_RING = 64,
    KC_PROC_PID_MAX = 0x10000,
    KC_SNAP_INITIAL = 256,

    KC_LC_RUNNING = 0,
    KC_LC_DRAINING = 1,
    KC_LC_DEAD = 2,

    KC_FLT_UNINIT = 0,
    KC_FLT_REGISTERED = 1,
    KC_FLT_STARTED = 2,
    KC_FLT_FAILED = 3
};

typedef struct _KC_FILE_DISPOSITION {
    BOOLEAN DeleteFile;
} KC_FILE_DISPOSITION;

typedef struct _KC_KEY_VALUE_PARTIAL {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataLength;
    UCHAR Data[1];
} KC_KEY_VALUE_PARTIAL;

typedef struct _KC_DIR_ENTRY {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    WCHAR         FileName[1];
} KC_DIR_ENTRY;

typedef NTSTATUS(NTAPI* PFN_ZW_QUERY_INFO_PROC)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* PFN_ZW_SET_INFO_PROC)(HANDLE, ULONG, PVOID, ULONG);
typedef NTSTATUS(NTAPI* PFN_ZW_QUERY_VALUE_KEY)(HANDLE, PUNICODE_STRING, ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* PFN_ZW_QUERY_DIR_FILE)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK,
    PVOID, ULONG, ULONG, BOOLEAN, PUNICODE_STRING, BOOLEAN);
typedef NTSTATUS(NTAPI* PFN_ZW_SET_INFO_FILE)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
typedef NTSTATUS(NTAPI* PFN_PS_LOOKUP_PROC_BY_PID)(HANDLE, PEPROCESS*);
typedef UCHAR(NTAPI* PFN_PS_GET_PROC_PROTECTION)(PEPROCESS);
typedef PCHAR(NTAPI* PFN_PS_GET_PROC_IMAGE)(PEPROCESS);

typedef struct _KC_APIS {
    PFN_ZW_QUERY_INFO_PROC     ZwQueryInformationProcess;
    PFN_ZW_SET_INFO_PROC       ZwSetInformationProcess;
    PFN_ZW_QUERY_VALUE_KEY     ZwQueryValueKey;
    PFN_ZW_QUERY_DIR_FILE      ZwQueryDirectoryFile;
    PFN_ZW_SET_INFO_FILE       ZwSetInformationFile;
    PFN_PS_LOOKUP_PROC_BY_PID  PsLookupProcessByProcessId;
    PFN_PS_GET_PROC_PROTECTION PsGetProcessProtection;
    PFN_PS_GET_PROC_IMAGE      PsGetProcessImageFileName;
} KC_APIS;

typedef struct _KC_LIFECYCLE {
    KSPIN_LOCK     StateLock;
    KEVENT         DrainEvent;
    volatile LONG  ActiveRequests;
    volatile LONG  State;
    ULONG          BuildNumber;
} KC_LIFECYCLE;

typedef struct _KC_GATE {
    volatile LONG SnapshotOps;
} KC_GATE;

typedef struct _KC_LEGACY_QUEUE {
    KSPIN_LOCK     Lock;
    KC_EVENT       Events[KC_RING];
    ULONG          Head;
    ULONG          Count;
    volatile LONG  Dropped;
} KC_LEGACY_QUEUE;

typedef struct _KC_FANOUT_QUEUE {
    KSPIN_LOCK     Lock;
    KC_EVENT       Events[KC_ENUM_SLOTS][KC_PORT_RING];
    ULONG          Head[KC_ENUM_SLOTS];
    ULONG          Count[KC_ENUM_SLOTS];
    volatile LONG  Dropped[KC_ENUM_SLOTS];
    ULONG          Owner[KC_ENUM_SLOTS];
    ULONG          LastUse[KC_ENUM_SLOTS];
    ULONG          Tick;
    PFLT_PORT      ServerPort;
    BOOLEAN        PortOk;
} KC_FANOUT_QUEUE;

typedef struct _KC_ENUM_SESSION {
    BOOLEAN       Open;
    BOOLEAN       Building;
    ULONG         OwnerPid;
    ULONG         Kind;
    ULONG         Session;
    ULONG         LastUse;
    KC_ENUM_ITEM* Snap;
    ULONG         Count;
} KC_ENUM_SESSION;

typedef struct _KC_ENUM_MANAGER {
    ERESOURCE        Lock;
    KC_ENUM_SESSION  S[KC_ENUM_SLOTS];
    ULONG            Epoch;
    ULONG            Tick;
    ULONG            OpenCount;
} KC_ENUM_MANAGER;

typedef struct _KC_CB_STATE {
    ERESOURCE      Lock;
    ULONG          Mask;
    ULONG          Generation;
    LARGE_INTEGER  CmCookie;
    PVOID          ObHandle;
} KC_CB_STATE;

typedef struct _KC_FLT_STATE {
    PFLT_FILTER    Filter;
    BOOLEAN        Started;
    volatile LONG  InstanceCount;
    volatile LONG  Ops[KC_FLT_OPS];
    ULONG          State;
    kc_u32         LastStatus;
} KC_FLT_STATE;

typedef struct _KC_FP_STATE {
    KSPIN_LOCK     Lock;
    WCHAR          Rules[KC_FP_MAX][KC_MAX_PATH];
    ULONG          RuleLen[KC_FP_MAX];
    BOOLEAN        RuleActive[KC_FP_MAX];
    volatile LONG  RuleCount;
    volatile LONG  Blocked;
    volatile LONG  Observed;
} KC_FP_STATE;

typedef struct _KC_PROT_STATE {
    ERESOURCE   StripLock;
    ERESOURCE   CriticalLock;
    ERESOURCE   TermLock;
    KSPIN_LOCK  OffsetLock;
    ULONG       Offset;
    ULONG       OffsetBuild;
    BOOLEAN     OffsetValid;
} KC_PROT_STATE;

typedef struct _KC_FILE_STATE {
    ERESOURCE RdLock;
} KC_FILE_STATE;

typedef struct _KC_PATH_VIEW {
    kc_u32 Status;
    WCHAR   Buf[KC_MAX_PATH];
} KC_PATH_VIEW;

typedef struct _KC_STATE {
    PDEVICE_OBJECT   DeviceObject;
    KC_APIS          Apis;
    KC_LIFECYCLE     Lifecycle;
    KC_GATE          Gate;
    KC_LEGACY_QUEUE  Legacy;
    KC_FANOUT_QUEUE  Fanout;
    KC_ENUM_MANAGER  Enum;
    KC_CB_STATE      Cb;
    KC_FLT_STATE     Flt;
    KC_FP_STATE      Fp;
    KC_PROT_STATE    Prot;
    KC_FILE_STATE    File;
} KC_STATE;

static KC_STATE g;

static UNICODE_STRING g_DevName;
static UNICODE_STRING g_SymLink;
static UNICODE_STRING g_Sddl;

static VOID     KcEmitEvent(ULONG Type, ULONG Pid, ULONG Op, NTSTATUS St, const WCHAR* Detail);
static NTSTATUS KcFltReady(VOID);
static VOID     KcFreeSnap(ULONG slot);
static BOOLEAN  KcProtectionOffsetCached(ULONG* out);
static kc_u32   KcEnumOpen(ULONG kind, const WCHAR* path, ULONG ownerPid, kc_u32 flags,
    KC_ENUM_OPEN_RESULT* out);
static kc_u32   KcEnumPage(ULONG session, ULONG cursor, ULONG kind, ULONG ownerPid,
    KC_ENUM_PAGE* page);
static kc_u32   KcEnumClose(ULONG session, ULONG ownerPid);
static VOID     KcFanoutStop(VOID);
static VOID     KcFltStop(VOID);
static VOID     KcCallbacksDisableAll(VOID);

static VOID KcInitNames(VOID)
{
    RtlInitUnicodeString(&g_DevName, L"\\Device\\Kernel_Tools");
    RtlInitUnicodeString(&g_SymLink, L"\\DosDevices\\Kernel_Tools");
    RtlInitUnicodeString(&g_Sddl, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
}

static ULONG KcWcsLen(const WCHAR* s)
{
    ULONG n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static BOOLEAN KcEqN(const WCHAR* a, const WCHAR* b, ULONG n)
{
    ULONG i;
    for (i = 0; i < n; ++i) {
        if (!a[i] || !b[i]) return FALSE;
        if (a[i] != b[i]) return FALSE;
    }
    return b[n] == L'\0';
}

static WCHAR KcLower(WCHAR c)
{
    return (c >= L'A' && c <= L'Z') ? (WCHAR)(c - L'A' + L'a') : c;
}

static BOOLEAN KcEqNI(const WCHAR* a, const WCHAR* b, ULONG n)
{
    ULONG i;
    for (i = 0; i < n; ++i) {
        if (!a[i] || !b[i]) return FALSE;
        if (KcLower(a[i]) != KcLower(b[i])) return FALSE;
    }
    return b[n] == L'\0';
}

static BOOLEAN KcStarts(const WCHAR* s, const WCHAR* p)
{
    return KcEqN(s, p, KcWcsLen(p));
}

static ULONG KcPack(kc_u16* dst, ULONG cap, const WCHAR* src)
{
    ULONG i = 0;
    if (!dst || cap == 0) return 0;
    if (src) {
        for (; i + 1 < cap && src[i]; ++i) dst[i] = (kc_u16)src[i];
    }
    dst[i] = 0;
    return i;
}

static PVOID KcAlloc(SIZE_T n)
{
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, n, KC_TAG);
}

static VOID KcFree(PVOID p)
{
    if (p) ExFreePoolWithTag(p, KC_TAG);
}

static kc_u32 KcNtToKc(NTSTATUS st)
{
    return NT_SUCCESS(st) ? KC_OK : (kc_u32)st;
}

static PVOID KcGetProc(const WCHAR* name)
{
    UNICODE_STRING u;
    RtlZeroMemory(&u, sizeof(u));
    RtlInitUnicodeString(&u, name);
    return MmGetSystemRoutineAddress(&u);
}

static VOID KcInitApis(VOID)
{
    g.Apis.ZwQueryInformationProcess =
        (PFN_ZW_QUERY_INFO_PROC)KcGetProc(L"ZwQueryInformationProcess");
    g.Apis.ZwSetInformationProcess =
        (PFN_ZW_SET_INFO_PROC)KcGetProc(L"ZwSetInformationProcess");
    g.Apis.ZwQueryValueKey =
        (PFN_ZW_QUERY_VALUE_KEY)KcGetProc(L"ZwQueryValueKey");
    g.Apis.ZwQueryDirectoryFile =
        (PFN_ZW_QUERY_DIR_FILE)KcGetProc(L"ZwQueryDirectoryFile");
    g.Apis.ZwSetInformationFile =
        (PFN_ZW_SET_INFO_FILE)KcGetProc(L"ZwSetInformationFile");
    g.Apis.PsLookupProcessByProcessId =
        (PFN_PS_LOOKUP_PROC_BY_PID)KcGetProc(L"PsLookupProcessByProcessId");
    g.Apis.PsGetProcessProtection =
        (PFN_PS_GET_PROC_PROTECTION)KcGetProc(L"PsGetProcessProtection");
    g.Apis.PsGetProcessImageFileName =
        (PFN_PS_GET_PROC_IMAGE)KcGetProc(L"PsGetProcessImageFileName");
}

static ULONG KcCurrentBuild(VOID)
{
    RTL_OSVERSIONINFOW v;
    RtlZeroMemory(&v, sizeof(v));
    v.dwOSVersionInfoSize = sizeof(v);
    if (!NT_SUCCESS(RtlGetVersion(&v))) return 0;
    return v.dwBuildNumber;
}

static NTSTATUS KcGrowArray(PVOID* arr, ULONG* cap, ULONG need, ULONG elem)
{
    PVOID p;
    ULONG nc;
    if (need <= *cap) return STATUS_SUCCESS;
    nc = *cap ? *cap * 2 : (ULONG)KC_SNAP_INITIAL;
    while (nc < need) nc *= 2;
    p = KcAlloc((SIZE_T)nc * (SIZE_T)elem);
    if (!p) return STATUS_INSUFFICIENT_RESOURCES;
    if (*arr) {
        RtlCopyMemory(p, *arr, (SIZE_T)(*cap) * (SIZE_T)elem);
        KcFree(*arr);
    }
    *arr = p;
    *cap = nc;
    return STATUS_SUCCESS;
}

static kc_u32 KcUnpackTo(KC_PATH_VIEW* v, const kc_u16* src, ULONG len)
{
    ULONG i;
    v->Buf[0] = L'\0';
    if (len >= KC_MAX_PATH) {
        v->Status = KC_ERR_PATH_LONG;
        return v->Status;
    }
    for (i = 0; i < len; ++i) v->Buf[i] = (WCHAR)src[i];
    v->Buf[len] = L'\0';
    v->Status = KC_OK;
    return v->Status;
}

static VOID KcNormalizePath(WCHAR* path, ULONG cap)
{
    WCHAR norm[KC_MAX_PATH] = { 0 };
    ULONG j, m;
    if (!KcStarts(path, L"\\DosDevices\\")) return;
    norm[0] = L'\\'; norm[1] = L'?'; norm[2] = L'?'; norm[3] = L'\\';
    for (j = 12; path[j] && (4 + j - 12) < cap - 1; ++j)
        norm[4 + j - 12] = path[j];
    m = 4 + j - 12;
    norm[m] = L'\0';
    for (j = 0; j <= m; ++j) path[j] = norm[j];
}

static kc_u32 KcValidatePath(WCHAR* path, ULONG cap)
{
    ULONG n, i;
    if (!path || cap < KC_MAX_PATH) return KC_ERR_PATH_SYNTAX;
    n = KcWcsLen(path);
    if (n == 0) return KC_ERR_EMPTY_PATH;
    if (n >= KC_MAX_PATH) return KC_ERR_PATH_LONG;
    if (path[0] != L'\\') return KC_ERR_PATH_NOT_ABS;

    if (n >= 6 && path[0] == L'\\' && path[1] == L'?' && path[2] == L'?' &&
        path[3] == L'\\' && path[5] == L':') {
        return KC_ERR_PATH_DRIVE;
    }
    if (!KcStarts(path, L"\\Device\\") && !KcStarts(path, L"\\??\\"))
        return KC_ERR_PATH_NOT_ABS;

    for (i = 1; i < n; ++i) {
        if (path[i] == L'\\' && path[i - 1] == L'\\') return KC_ERR_PATH_SYNTAX;
        if (path[i] == L'.' && path[i + 1] == L'.' &&
            (path[i - 1] == L'\\') &&
            (path[i + 2] == L'\\' || path[i + 2] == L'\0'))
            return KC_ERR_PATH_SYNTAX;
        if (path[i] == L'/') return KC_ERR_PATH_SYNTAX;
        if (path[i] == L'"' || path[i] == L':') return KC_ERR_PATH_SYNTAX;
    }
    return KC_OK;
}

static kc_u32 KcValidateRegPath(WCHAR* path, ULONG cap)
{
    static const WCHAR* deny[] = {
        L"SECURITY", L"SAM", L"BCD00000000", L"LSA"
    };
    ULONG n, i, k;
    BOOLEAN hasTail = FALSE, seenNonZero = FALSE;

    if (!path || cap < KC_MAX_PATH) return KC_ERR_REG_SYNTAX;
    n = KcWcsLen(path);
    if (n == 0) return KC_ERR_EMPTY_PATH;
    if (n >= KC_MAX_PATH) return KC_ERR_PATH_LONG;
    if (path[0] != L'\\') return KC_ERR_REG_NOT_ABS;
    if (!KcStarts(path, L"\\Registry\\")) return KC_ERR_REG_NOT_ABS;

    if (KcStarts(path, L"\\Registry\\Machine\\")) {
        hasTail = TRUE;
    }
    else if (KcStarts(path, L"\\Registry\\User\\")) {
        const WCHAR* tail = path + 15;
        if (!tail[0]) return KC_ERR_REG_CONTEXT;
        if (KcStarts(tail, L".DEFAULT") || KcStarts(tail, L"S-1-")) hasTail = TRUE;
        else return KC_ERR_REG_CONTEXT;
    }
    else return KC_ERR_REG_NOT_ABS;
    if (!hasTail) return KC_ERR_REG_CONTEXT;

    for (i = 1; i < n; ++i) {
        if (path[i] == L'\\' && path[i - 1] == L'\\') return KC_ERR_REG_SYNTAX;
        if (path[i] == L'.' && path[i + 1] == L'.' && path[i - 1] == L'\\' &&
            (path[i + 2] == L'\\' || path[i + 2] == L'\0')) return KC_ERR_REG_SYNTAX;
        if (path[i] == L'/') return KC_ERR_REG_SYNTAX;
        if (path[i] != L'\\' && path[i] != L'.' && path[i] != L'-') seenNonZero = TRUE;
    }
    if (!seenNonZero) return KC_ERR_REG_SYNTAX;

    for (k = 0; k < sizeof(deny) / sizeof(deny[0]); ++k) {
        const WCHAR* d = deny[k];
        ULONG dl = KcWcsLen(d);
        for (i = 0; i + dl <= n; ++i) {
            if ((i == 0 || path[i - 1] == L'\\') &&
                KcEqNI(path + i, d, dl) &&
                (path[i + dl] == L'\\' || path[i + dl] == L'\0'))
                return KC_ERR_REG_DENIED;
        }
    }
    return KC_OK;
}

static BOOLEAN KcApplyWow32To(const WCHAR* src, WCHAR* dst, ULONG cap)
{
    static const WCHAR prefix[] = L"\\Registry\\Machine\\SOFTWARE\\";
    static const WCHAR node[] = L"WOW6432Node\\";
    ULONG pl = KcWcsLen(prefix), nl = KcWcsLen(node), n, i;

    RtlZeroMemory(dst, (SIZE_T)cap * sizeof(WCHAR));
    if (!KcStarts(src, prefix)) {
        for (i = 0; i < cap && src[i]; ++i) dst[i] = src[i];
        return TRUE;
    }
    if (KcStarts(src + pl, node)) {
        for (i = 0; i < cap && src[i]; ++i) dst[i] = src[i];
        return TRUE;
    }
    n = KcWcsLen(src);
    if ((SIZE_T)n + nl + 1 >= cap) return FALSE;
    for (i = 0; i < pl; ++i) dst[i] = src[i];
    for (i = 0; i < nl; ++i) dst[pl + i] = node[i];
    for (i = pl; i <= n; ++i) dst[i + nl] = src[i];
    return TRUE;
}

static kc_u32 KcFilePipeline(KC_PATH_VIEW* v, const kc_u16* src, ULONG len)
{
    kc_u32 s = KcUnpackTo(v, src, len);
    if (s != KC_OK) return s;
    KcNormalizePath(v->Buf, KC_MAX_PATH);
    s = KcValidatePath(v->Buf, KC_MAX_PATH);
    v->Status = s;
    return s;
}

static kc_u32 KcRegPipeline(KC_PATH_VIEW* v, const kc_u16* src, ULONG len, BOOLEAN wow32)
{
    WCHAR tmp[KC_MAX_PATH] = { 0 };
    ULONG i;
    kc_u32 s = KcUnpackTo(v, src, len);
    if (s != KC_OK) return s;
    if (wow32) {
        if (!KcApplyWow32To(v->Buf, tmp, KC_MAX_PATH)) {
            v->Status = KC_ERR_PATH_LONG;
            return v->Status;
        }
        for (i = 0; i < KC_MAX_PATH && tmp[i]; ++i) v->Buf[i] = tmp[i];
        v->Buf[KC_MAX_PATH - 1] = L'\0';
    }
    s = KcValidateRegPath(v->Buf, KC_MAX_PATH);
    v->Status = s;
    return s;
}

static VOID KcInitOa(POBJECT_ATTRIBUTES oa, PUNICODE_STRING us, const WCHAR* path)
{
    RtlInitUnicodeString(us, path);
    InitializeObjectAttributes(oa, us, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
}

static VOID KcInitSync(VOID)
{
    ULONG i;
    KeInitializeSpinLock(&g.Lifecycle.StateLock);
    KeInitializeEvent(&g.Lifecycle.DrainEvent, NotificationEvent, TRUE);
    g.Lifecycle.ActiveRequests = 0;
    g.Lifecycle.State = KC_LC_RUNNING;
    g.Lifecycle.BuildNumber = KcCurrentBuild();
    g.Flt.State = KC_FLT_UNINIT;
    g.Flt.LastStatus = (kc_u32)STATUS_NOT_SUPPORTED;

    KeInitializeSpinLock(&g.Legacy.Lock);
    KeInitializeSpinLock(&g.Fanout.Lock);
    KeInitializeSpinLock(&g.Fp.Lock);
    KeInitializeSpinLock(&g.Prot.OffsetLock);

    ExInitializeResourceLite(&g.Enum.Lock);
    ExInitializeResourceLite(&g.Cb.Lock);
    ExInitializeResourceLite(&g.Prot.StripLock);
    ExInitializeResourceLite(&g.Prot.CriticalLock);
    ExInitializeResourceLite(&g.Prot.TermLock);
    ExInitializeResourceLite(&g.File.RdLock);

    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
        g.Fanout.Owner[i] = 0;
        g.Fanout.Head[i] = 0;
        g.Fanout.Count[i] = 0;
        g.Fanout.Dropped[i] = 0;
        g.Fanout.LastUse[i] = 0;
        RtlZeroMemory(&g.Enum.S[i], sizeof(g.Enum.S[i]));
    }
}

static VOID KcDeleteSync(VOID)
{
    ULONG i;
    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) KcFreeSnap(i);
    ExDeleteResourceLite(&g.File.RdLock);
    ExDeleteResourceLite(&g.Prot.TermLock);
    ExDeleteResourceLite(&g.Prot.CriticalLock);
    ExDeleteResourceLite(&g.Prot.StripLock);
    ExDeleteResourceLite(&g.Cb.Lock);
    ExDeleteResourceLite(&g.Enum.Lock);
}

static BOOLEAN KcRequestEnter(VOID)
{
    KIRQL irql;
    BOOLEAN ok;
    KeAcquireSpinLock(&g.Lifecycle.StateLock, &irql);
    if (g.Lifecycle.State != KC_LC_RUNNING) {
        ok = FALSE;
    }
    else {
        ++g.Lifecycle.ActiveRequests;
        KeResetEvent(&g.Lifecycle.DrainEvent);
        ok = TRUE;
    }
    KeReleaseSpinLock(&g.Lifecycle.StateLock, irql);
    return ok;
}

static VOID KcRequestExit(VOID)
{
    KIRQL irql;
    KeAcquireSpinLock(&g.Lifecycle.StateLock, &irql);
    --g.Lifecycle.ActiveRequests;
    if (g.Lifecycle.ActiveRequests == 0)
        KeSetEvent(&g.Lifecycle.DrainEvent, 0, FALSE);
    KeReleaseSpinLock(&g.Lifecycle.StateLock, irql);
}

static VOID KcRequestDrain(VOID)
{
    KIRQL irql;
    BOOLEAN needWait;
    KeAcquireSpinLock(&g.Lifecycle.StateLock, &irql);
    g.Lifecycle.State = KC_LC_DRAINING;
    needWait = (g.Lifecycle.ActiveRequests != 0);
    KeReleaseSpinLock(&g.Lifecycle.StateLock, irql);
    if (needWait)
        (VOID)KeWaitForSingleObject(&g.Lifecycle.DrainEvent, Executive,
            KernelMode, FALSE, NULL);
}

static VOID KcGateEnterSnapshot(VOID)
{
    InterlockedIncrement(&g.Gate.SnapshotOps);
}

static VOID KcGateLeaveSnapshot(VOID)
{
    InterlockedDecrement(&g.Gate.SnapshotOps);
}

static BOOLEAN KcGateBlocks(ULONG code)
{
    if (!g.Gate.SnapshotOps) return FALSE;
    switch (code) {
    case IOCTL_KC_DEL:
    case IOCTL_KC_COPY:
    case IOCTL_KC_RD:
    case IOCTL_KC_KILL:
    case IOCTL_KC_STRIP_PROTECTION:
    case IOCTL_KC_CRITICAL:
    case IOCTL_KC_CALLBACK_ON:
    case IOCTL_KC_CALLBACK_OFF:
    case IOCTL_KC_FPROTECT:
        return TRUE;
    default:
        return FALSE;
    }
}

static VOID KcLegacyPush(const KC_EVENT* e)
{
    KIRQL irql;
    ULONG idx;
    KeAcquireSpinLock(&g.Legacy.Lock, &irql);
    if (g.Legacy.Count >= KC_RING) {
        g.Legacy.Head = (g.Legacy.Head + 1) % KC_RING;
        g.Legacy.Count = KC_RING - 1;
        InterlockedIncrement(&g.Legacy.Dropped);
    }
    idx = (g.Legacy.Head + g.Legacy.Count) % KC_RING;
    g.Legacy.Events[idx] = *e;
    ++g.Legacy.Count;
    KeReleaseSpinLock(&g.Legacy.Lock, irql);
}

static ULONG KcLegacyPull(KC_EVENT_BATCH* b)
{
    KIRQL irql;
    ULONG n, i;
    RtlZeroMemory(b, sizeof(*b));
    KeAcquireSpinLock(&g.Legacy.Lock, &irql);
    n = g.Legacy.Count < KC_BATCH_MAX ? g.Legacy.Count : (ULONG)KC_BATCH_MAX;
    for (i = 0; i < n; ++i)
        b->Events[i] = g.Legacy.Events[(g.Legacy.Head + i) % KC_RING];
    b->Count = n;
    b->Reserved = (kc_u32)InterlockedExchange(&g.Legacy.Dropped, 0);
    g.Legacy.Head = (g.Legacy.Head + n) % KC_RING;
    g.Legacy.Count -= n;
    KeReleaseSpinLock(&g.Legacy.Lock, irql);
    return n;
}

static ULONG KcFanoutSlotForCurrent(VOID)
{
    ULONG pid = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
    ULONG i, free = KC_ENUM_SLOTS, oldest = 0, tick, best;
    KIRQL irql;
    KeAcquireSpinLock(&g.Fanout.Lock, &irql);
    tick = ++g.Fanout.Tick;
    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
        if (g.Fanout.Owner[i] == pid) {
            g.Fanout.LastUse[i] = tick;
            KeReleaseSpinLock(&g.Fanout.Lock, irql);
            return i;
        }
        if (!g.Fanout.Owner[i] && free == (ULONG)KC_ENUM_SLOTS) free = i;
    }
    if (free == (ULONG)KC_ENUM_SLOTS) {
        best = 0xffffffffu;
        for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
            if ((ULONG)(tick - g.Fanout.LastUse[i]) < best) {
                best = (ULONG)(tick - g.Fanout.LastUse[i]);
                oldest = i;
            }
        }
        free = oldest;
        g.Fanout.Count[free] = 0;
        g.Fanout.Head[free] = 0;
    }
    g.Fanout.Owner[free] = pid;
    g.Fanout.LastUse[free] = tick;
    KeReleaseSpinLock(&g.Fanout.Lock, irql);
    return free;
}

static VOID KcFanoutPush(const KC_EVENT* e)
{
    ULONG i;
    KIRQL irql;
    KeAcquireSpinLock(&g.Fanout.Lock, &irql);
    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
        ULONG idx;
        if (!g.Fanout.Owner[i]) continue;
        if (g.Fanout.Count[i] >= KC_PORT_RING) {
            g.Fanout.Head[i] = (g.Fanout.Head[i] + 1) % KC_PORT_RING;
            g.Fanout.Count[i] = KC_PORT_RING - 1;
            InterlockedIncrement(&g.Fanout.Dropped[i]);
        }
        idx = (g.Fanout.Head[i] + g.Fanout.Count[i]) % KC_PORT_RING;
        g.Fanout.Events[i][idx] = *e;
        ++g.Fanout.Count[i];
    }
    KeReleaseSpinLock(&g.Fanout.Lock, irql);
}

static ULONG KcFanoutPull(KC_EVENT_BATCH* b)
{
    ULONG slot = KcFanoutSlotForCurrent();
    ULONG n, i;
    KIRQL irql;
    RtlZeroMemory(b, sizeof(*b));
    KeAcquireSpinLock(&g.Fanout.Lock, &irql);
    n = g.Fanout.Count[slot];
    if (n > (ULONG)KC_BATCH_MAX) n = (ULONG)KC_BATCH_MAX;
    for (i = 0; i < n; ++i)
        b->Events[i] = g.Fanout.Events[slot][(g.Fanout.Head[slot] + i) % KC_PORT_RING];
    b->Count = n;
    b->Reserved = (kc_u32)InterlockedExchange(&g.Fanout.Dropped[slot], 0);
    g.Fanout.Head[slot] = (g.Fanout.Head[slot] + n) % KC_PORT_RING;
    g.Fanout.Count[slot] -= n;
    KeReleaseSpinLock(&g.Fanout.Lock, irql);
    return n;
}

static VOID KcFanoutStop(VOID)
{
    ULONG i;
    KIRQL irql;
    if (g.Fanout.ServerPort) {
        FltCloseCommunicationPort(g.Fanout.ServerPort);
        g.Fanout.ServerPort = NULL;
    }
    KeAcquireSpinLock(&g.Fanout.Lock, &irql);
    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
        g.Fanout.Owner[i] = 0;
        g.Fanout.Count[i] = 0;
        g.Fanout.Head[i] = 0;
    }
    g.Fanout.PortOk = FALSE;
    KeReleaseSpinLock(&g.Fanout.Lock, irql);
}

static VOID KcEmitEvent(ULONG Type, ULONG Pid, ULONG Op, NTSTATUS St, const WCHAR* Detail)
{
    WCHAR tmp[KC_DETAIL] = { 0 };
    KC_EVENT e;
    LARGE_INTEGER ts = { 0 };
    ULONG i = 0;
    if (Detail) {
        for (; i + 1 < KC_DETAIL && Detail[i]; ++i) tmp[i] = Detail[i];
        tmp[i] = L'\0';
    }
    KeQuerySystemTime(&ts);
    RtlZeroMemory(&e, sizeof(e));
    e.TimeStamp = (kc_u64)ts.QuadPart;
    e.Type = Type;
    e.ProcessId = Pid;
    e.Operation = Op;
    e.Status = (kc_u32)St;
    (VOID)KcPack(e.Detail, KC_DETAIL, tmp);
    KcLegacyPush(&e);
    KcFanoutPush(&e);
}

static NTSTATUS FLTAPI KcPortConnect(
    PFLT_PORT ClientPort,
    PVOID ServerPortCookie,
    PVOID ConnectionContext,
    ULONG SizeOfContext,
    PVOID* ConnectionCookie)
{
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    if (ConnectionCookie) *ConnectionCookie = NULL;
    return STATUS_SUCCESS;
}

static VOID FLTAPI KcPortDisconnect(PVOID ConnectionCookie)
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
}

static NTSTATUS FLTAPI KcPortMessage(
    PVOID ConnectionCookie,
    PVOID SenderBuffer,
    ULONG SenderBufferLength,
    PVOID ReplyBuffer,
    PULONG ReplyLength)
{
    UNREFERENCED_PARAMETER(ConnectionCookie);
    UNREFERENCED_PARAMETER(SenderBuffer);
    UNREFERENCED_PARAMETER(SenderBufferLength);
    UNREFERENCED_PARAMETER(ReplyBuffer);
    if (ReplyLength) *ReplyLength = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS KcPortInit(VOID)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING name = { 0 };
    PSECURITY_DESCRIPTOR sd = NULL;
    NTSTATUS st;
    if (!g.Flt.Filter) return STATUS_NOT_SUPPORTED;
    RtlInitUnicodeString(&name, L"\\Kernel_ToolsEventPort");
    st = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
    if (!NT_SUCCESS(st)) return st;
    InitializeObjectAttributes(&oa, &name,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);
    st = FltCreateCommunicationPort(g.Flt.Filter, &g.Fanout.ServerPort, &oa, NULL,
        (PFLT_CONNECT_NOTIFY)KcPortConnect,
        (PFLT_DISCONNECT_NOTIFY)KcPortDisconnect,
        (PFLT_MESSAGE_NOTIFY)KcPortMessage, 2);
    FltFreeSecurityDescriptor(sd);
    if (!NT_SUCCESS(st)) g.Fanout.ServerPort = NULL;
    else g.Fanout.PortOk = TRUE;
    return st;
}

typedef struct _KC_SAMPLE {
    PEPROCESS Proc;
    UCHAR     Truth;
} KC_SAMPLE;

static ULONG KcScanInstruction(PUCHAR p)
{
    ULONG i;
    if (!p) return 0;
    for (i = 0; i + 7 <= KC_SCAN_BYTES; ++i) {
        ULONG v = 0;
        BOOLEAN fault = FALSE, hit = FALSE;
        __try {
            if (p[i] == 0x0F && p[i + 1] == 0xB6 &&
                (p[i + 2] == 0x81 || p[i + 2] == 0x89 || p[i + 2] == 0x91)) {
                RtlCopyMemory(&v, p + i + 3, sizeof(v));
                hit = TRUE;
            }
            else if (p[i] == 0x8A && (p[i + 1] == 0x81 || p[i + 1] == 0x89)) {
                RtlCopyMemory(&v, p + i + 2, sizeof(v));
                hit = TRUE;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { fault = TRUE; }
        if (fault) return 0;
        if (hit) return v;
    }
    return 0;
}

static ULONG KcCollectSamples(KC_SAMPLE* s, ULONG cap)
{
    ULONG n = 0, pid;
    if (!g.Apis.PsGetProcessProtection || !g.Apis.PsLookupProcessByProcessId) return 0;
    for (pid = 4; pid < 0x10000 && n < cap; pid += 4) {
        PEPROCESS p = NULL;
        if (!NT_SUCCESS(g.Apis.PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &p))) continue;
        s[n].Proc = p;
        s[n].Truth = g.Apis.PsGetProcessProtection(p);
        ++n;
    }
    return n;
}

static VOID KcReleaseSamples(KC_SAMPLE* s, ULONG n)
{
    ULONG i;
    for (i = 0; i < n; ++i) ObDereferenceObject(s[i].Proc);
}

static BOOLEAN KcOffsetMatches(const KC_SAMPLE* s, ULONG n, ULONG off)
{
    ULONG i;
    if (off == 0 || off > KC_MAX_OFFSET_SCAN) return FALSE;
    for (i = 0; i < n; ++i) {
        UCHAR v = 0;
        BOOLEAN fault = FALSE;
        __try { v = *(volatile UCHAR*)((PUCHAR)s[i].Proc + off); }
        __except (EXCEPTION_EXECUTE_HANDLER) { fault = TRUE; }
        if (fault || v != s[i].Truth) return FALSE;
    }
    return TRUE;
}

static BOOLEAN KcResolveProtectionOffset(ULONG* out)
{
    KC_SAMPLE s[KC_SAMPLE_MAX] = { 0 };
    ULONG n, i, cand, off, first = 0, matches = 0;
    BOOLEAN hasNonZero = FALSE, ok = FALSE;
    *out = 0;
    if (!g.Apis.PsGetProcessProtection || !g.Apis.ZwQueryInformationProcess) return FALSE;
    n = KcCollectSamples(s, KC_SAMPLE_MAX);
    if (n < 3) { KcReleaseSamples(s, n); return FALSE; }
    for (i = 0; i < n; ++i) if (s[i].Truth != 0) hasNonZero = TRUE;
    if (!hasNonZero) { KcReleaseSamples(s, n); return FALSE; }

    cand = KcScanInstruction((PUCHAR)g.Apis.PsGetProcessProtection);
    if (cand && KcOffsetMatches(s, n, cand)) {
        *out = cand;
        ok = TRUE;
    }
    else {
        for (off = 1; off < KC_MAX_OFFSET_SCAN; ++off) {
            if (KcOffsetMatches(s, n, off)) {
                if (matches == 0) first = off;
                ++matches;
                if (matches > 1) break;
            }
        }
        if (matches == 1) { *out = first; ok = TRUE; }
    }
    KcReleaseSamples(s, n);
    return ok;
}

static BOOLEAN KcProtectionOffsetCached(ULONG* out)
{
    KIRQL irql;
    ULONG off = 0, build = 0;
    BOOLEAN valid, ok;
    KeAcquireSpinLock(&g.Prot.OffsetLock, &irql);
    valid = g.Prot.OffsetValid;
    off = g.Prot.Offset;
    build = g.Prot.OffsetBuild;
    KeReleaseSpinLock(&g.Prot.OffsetLock, irql);

    if (valid && build == g.Lifecycle.BuildNumber) { *out = off; return TRUE; }

    ok = KcResolveProtectionOffset(&off);
    KeAcquireSpinLock(&g.Prot.OffsetLock, &irql);
    if (ok) {
        g.Prot.Offset = off;
        g.Prot.OffsetBuild = g.Lifecycle.BuildNumber;
        g.Prot.OffsetValid = TRUE;
    }
    else {
        g.Prot.Offset = 0;
        g.Prot.OffsetValid = FALSE;
    }
    KeReleaseSpinLock(&g.Prot.OffsetLock, irql);
    *out = ok ? off : 0;
    return ok;
}

static VOID KcProtectionOffsetCachedRead(ULONG* out)
{
    KIRQL irql;
    KeAcquireSpinLock(&g.Prot.OffsetLock, &irql);
    *out = (g.Prot.OffsetValid && g.Prot.OffsetBuild == g.Lifecycle.BuildNumber)
        ? g.Prot.Offset : 0;
    KeReleaseSpinLock(&g.Prot.OffsetLock, irql);
}

static BOOLEAN KcProcessGone(PEPROCESS p)
{
    LARGE_INTEGER t = { 0 };
    if (!p) return TRUE;
    t.QuadPart = 0;
    return KeWaitForSingleObject(p, Executive, KernelMode, FALSE, &t) == STATUS_SUCCESS;
}

static NTSTATUS KcOpenProcess(HANDLE pid, ACCESS_MASK access, PHANDLE out)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    CLIENT_ID cid = { 0 };
    *out = NULL;
    InitializeObjectAttributes(&oa, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
    cid.UniqueProcess = pid;
    cid.UniqueThread = NULL;
    return ZwOpenProcess(out, access, &oa, &cid);
}

static NTSTATUS KcQueryProtection(HANDLE pid, KC_PROTECT_INFO* info)
{
    HANDLE h = NULL;
    NTSTATUS st;
    UCHAR raw = 0;
    ULONG retLen = 0, off = 0;

    RtlZeroMemory(info, sizeof(*info));
    info->Pid = (kc_u32)(ULONG_PTR)pid;
    if (!g.Apis.ZwQueryInformationProcess) {
        info->Error = KC_ERR_API_MISSING;
        return STATUS_NOT_SUPPORTED;
    }
    st = KcOpenProcess(pid, KC_PROCESS_QUERY_INFORMATION, &h);
    if (!NT_SUCCESS(st)) { info->Error = KcNtToKc(st); return st; }
    st = g.Apis.ZwQueryInformationProcess(h, KC_INFO_PROTECTION,
        &raw, sizeof(raw), &retLen);
    if (NT_SUCCESS(st)) {
        info->Raw = raw;
        info->Type = (kc_u8)(raw & 0x7);
        info->Audit = (kc_u8)((raw >> 3) & 0x1);
        info->Signer = (kc_u8)((raw >> 4) & 0xF);
    }
    else info->Error = KcNtToKc(st);
    KcProtectionOffsetCachedRead(&off);
    info->ProtectionOffset = off;
    ZwClose(h);
    return st;
}

static NTSTATUS KcStripProtection(HANDLE pid, kc_u32 flags, KC_STRIP_RESULT* out)
{
    HANDLE h = NULL;
    PEPROCESS proc = NULL;
    NTSTATUS st = STATUS_SUCCESS;
    ULONG retLen = 0, off = 0, crit = 0;
    UCHAR protRaw = 0, before = 0, after = 0;
    BOOLEAN written = FALSE, restored = FALSE, fault;

    RtlZeroMemory(out, sizeof(*out));
    out->Pid = (kc_u32)(ULONG_PTR)pid;

    if (!g.Apis.ZwQueryInformationProcess || !g.Apis.PsLookupProcessByProcessId ||
        !g.Apis.PsGetProcessProtection) {
        out->Status = KC_ERR_API_MISSING;
        return STATUS_NOT_SUPPORTED;
    }

    ExAcquireResourceExclusiveLite(&g.Prot.StripLock, TRUE);

    st = KcOpenProcess(pid, KC_PROCESS_QUERY_INFORMATION, &h);
    if (!NT_SUCCESS(st)) { out->Status = KcNtToKc(st); goto done; }

    st = g.Apis.ZwQueryInformationProcess(h, KC_INFO_BREAK_ON_TERM,
        &crit, sizeof(crit), &retLen);
    if (!NT_SUCCESS(st)) crit = 0;
    out->WasCritical = crit ? 1u : 0u;
    if (crit && !(flags & KC_STRIP_ALLOW_CRITICAL)) {
        out->Status = KC_ERR_UNSAFE_TARGET;
        st = STATUS_ACCESS_DENIED;
        goto done;
    }

    st = g.Apis.ZwQueryInformationProcess(h, KC_INFO_PROTECTION,
        &protRaw, sizeof(protRaw), &retLen);
    if (!NT_SUCCESS(st)) protRaw = 0;
    if (protRaw == 0) {
        out->AlreadyClear = 1u;
        out->Status = KC_OK;
        goto done;
    }
    if (!KcProtectionOffsetCached(&off) || off == 0) {
        out->Status = KC_ERR_OFFSET_FAIL;
        st = STATUS_UNSUCCESSFUL;
        goto done;
    }
    out->ProtectionOffset = off;

    st = g.Apis.PsLookupProcessByProcessId(pid, &proc);
    if (!NT_SUCCESS(st)) {
        out->Status = KC_ERR_PPL_GONE;
        st = STATUS_OBJECT_NAME_NOT_FOUND;
        goto done;
    }
    if (KcProcessGone(proc)) {
        out->Status = KC_ERR_PPL_GONE;
        st = STATUS_OBJECT_NAME_NOT_FOUND;
        goto done;
    }

    fault = FALSE;
    __try { before = *(volatile UCHAR*)((PUCHAR)proc + off); }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault = TRUE; }
    if (fault) {
        out->Status = KC_ERR_OFFSET_FAIL;
        st = STATUS_UNSUCCESSFUL;
        goto done;
    }
    out->ProtectionBefore = (kc_u32)before;
    if (before != protRaw) {
        out->Status = KC_ERR_OFFSET_FAIL;
        st = STATUS_UNSUCCESSFUL;
        goto done;
    }

    written = TRUE;
    after = 0xffu;
    fault = FALSE;
    __try {
        *(volatile UCHAR*)((PUCHAR)proc + off) = 0;
        after = *(volatile UCHAR*)((PUCHAR)proc + off);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault = TRUE; }
    out->ProtectionAfter = (kc_u32)after;

    if (fault || after != 0) {
        __try { *(volatile UCHAR*)((PUCHAR)proc + off) = before; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        restored = TRUE;
        out->Restored = 1u;
        out->ProtectionAfter = (kc_u32)before;
        out->Status = KC_ERR_PPL_STRIP;
        st = STATUS_UNSUCCESSFUL;
        goto done;
    }

    out->Status = KC_OK;
    st = STATUS_SUCCESS;

done:

    if (written && !restored && st != STATUS_SUCCESS && proc) {
        __try { *(volatile UCHAR*)((PUCHAR)proc + off) = before; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        out->Restored = 1u;
        out->ProtectionAfter = (kc_u32)before;
    }
    if (proc) ObDereferenceObject(proc);
    if (h) ZwClose(h);
    ExReleaseResourceLite(&g.Prot.StripLock);
    return st;
}

static NTSTATUS KcCriticalOp(HANDLE pid, ULONG op, kc_u32 flags, KC_CRITICAL_RESULT* out)
{
    HANDLE h = NULL;
    NTSTATUS st = STATUS_SUCCESS;
    ULONG before = 0, after = 0, value = 0, retLen = 0;
    BOOLEAN written = FALSE, restored = FALSE;

    RtlZeroMemory(out, sizeof(*out));
    out->Pid = (kc_u32)(ULONG_PTR)pid;
    out->Operation = op;
    if (!g.Apis.ZwQueryInformationProcess || !g.Apis.ZwSetInformationProcess) {
        out->Status = KC_ERR_API_MISSING;
        return STATUS_NOT_SUPPORTED;
    }
    if (op != KC_CRIT_OP_QUERY && op != KC_CRIT_OP_SET && op != KC_CRIT_OP_CLEAR) {
        out->Status = KC_ERR_BAD_INPUT;
        return STATUS_INVALID_PARAMETER;
    }
    if (op == KC_CRIT_OP_CLEAR && !(flags & KC_CRIT_ALLOW_CLEAR)) {
        out->Status = KC_ERR_REFUSED;
        return STATUS_ACCESS_DENIED;
    }

    ExAcquireResourceExclusiveLite(&g.Prot.CriticalLock, TRUE);
    st = KcOpenProcess(pid,
        KC_PROCESS_QUERY_INFORMATION | KC_PROCESS_SET_INFORMATION, &h);
    if (!NT_SUCCESS(st)) { out->Status = KcNtToKc(st); goto done; }

    st = g.Apis.ZwQueryInformationProcess(h, KC_INFO_BREAK_ON_TERM,
        &before, sizeof(before), &retLen);
    if (!NT_SUCCESS(st)) { out->Status = KcNtToKc(st); goto done; }
    out->Before = before;

    if (op == KC_CRIT_OP_QUERY) {
        out->After = before;
        out->Status = KC_OK;
        goto done;
    }

    value = (op == KC_CRIT_OP_SET) ? 1u : 0u;
    written = TRUE;
    st = g.Apis.ZwSetInformationProcess(h, KC_INFO_BREAK_ON_TERM,
        &value, sizeof(value));
    if (!NT_SUCCESS(st)) {
        out->Status = KC_ERR_CRITICAL_CLEAR;
        goto done;
    }
    retLen = 0;
    st = g.Apis.ZwQueryInformationProcess(h, KC_INFO_BREAK_ON_TERM,
        &after, sizeof(after), &retLen);
    if (!NT_SUCCESS(st)) after = 0xffu;
    out->After = after;
    if (!NT_SUCCESS(st) || after != value) {
        ULONG orig = before;
        (VOID)g.Apis.ZwSetInformationProcess(h, KC_INFO_BREAK_ON_TERM,
            &orig, sizeof(orig));
        restored = TRUE;
        out->Restored = 1u;
        out->After = before;
        out->Status = KC_ERR_CRITICAL_CLEAR;
        st = STATUS_UNSUCCESSFUL;
        goto done;
    }
    out->Status = KC_OK;
    st = STATUS_SUCCESS;

done:
    if (written && !restored && st != STATUS_SUCCESS && h) {
        ULONG orig = before;
        (VOID)g.Apis.ZwSetInformationProcess(h, KC_INFO_BREAK_ON_TERM,
            &orig, sizeof(orig));
        out->Restored = 1u;
        out->After = before;
        if (out->Status == KC_OK) out->Status = KC_ERR_CRITICAL_CLEAR;
    }
    if (h) ZwClose(h);
    ExReleaseResourceLite(&g.Prot.CriticalLock);
    return st;
}

static NTSTATUS KcTerminate(HANDLE pid, kc_u32 flags, KC_TERM_RESULT* out)
{
    HANDLE h = NULL;
    NTSTATUS st = STATUS_SUCCESS;
    ULONG crit = 0, retLen = 0;

    RtlZeroMemory(out, sizeof(*out));
    out->Pid = (kc_u32)(ULONG_PTR)pid;
    if (flags & KC_KILL_NOPPL) {
        out->Status = KC_ERR_UNSUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }
    if (!g.Apis.ZwQueryInformationProcess) {
        out->Status = KC_ERR_API_MISSING;
        return STATUS_NOT_SUPPORTED;
    }

    ExAcquireResourceExclusiveLite(&g.Prot.TermLock, TRUE);
    st = KcOpenProcess(pid,
        KC_PROCESS_TERMINATE | KC_PROCESS_QUERY_INFORMATION, &h);
    if (!NT_SUCCESS(st)) { out->Status = KcNtToKc(st); goto done; }

    st = g.Apis.ZwQueryInformationProcess(h, KC_INFO_BREAK_ON_TERM,
        &crit, sizeof(crit), &retLen);
    if (!NT_SUCCESS(st)) crit = 0;
    out->WasCritical = crit ? 1u : 0u;

    if (crit && !(flags & KC_TERM_ALLOW_CRITICAL)) {
        out->Refused = 1u;
        out->Status = KC_ERR_CRITICAL_REFUSE;
        st = STATUS_ACCESS_DENIED;
        goto done;
    }
    st = ZwTerminateProcess(h, 1);
    out->Status = KcNtToKc(st);

done:
    if (h) ZwClose(h);
    ExReleaseResourceLite(&g.Prot.TermLock);
    return st;
}

static NTSTATUS KcQueryAttributes(const WCHAR* path, ULONG* attrs)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE h = NULL;
    FILE_BASIC_INFORMATION bi;
    NTSTATUS st;
    *attrs = 0;
    KcInitOa(&oa, &us, path);
    st = ZwCreateFile(&h, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT,
        NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    RtlZeroMemory(&bi, sizeof(bi));
    st = ZwQueryInformationFile(h, &ios, &bi, sizeof(bi), FileBasicInformation);
    if (NT_SUCCESS(st)) *attrs = bi.FileAttributes;
    ZwClose(h);
    return st;
}

static NTSTATUS KcClearReadOnly(const WCHAR* path)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE h = NULL;
    FILE_BASIC_INFORMATION bi;
    NTSTATUS st;
    KcInitOa(&oa, &us, path);
    st = ZwCreateFile(&h, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
        &oa, &ios, NULL, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    RtlZeroMemory(&bi, sizeof(bi));
    st = ZwQueryInformationFile(h, &ios, &bi, sizeof(bi), FileBasicInformation);
    if (NT_SUCCESS(st)) {
        bi.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
        st = ZwSetInformationFile(h, &ios, &bi, sizeof(bi), FileBasicInformation);
    }
    ZwClose(h);
    return st;
}

static NTSTATUS KcDeleteOne(const WCHAR* path)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE h = NULL;
    NTSTATUS st;
    KC_FILE_DISPOSITION disp = { 0 };
    if (!g.Apis.ZwSetInformationFile) return STATUS_NOT_SUPPORTED;
    KcInitOa(&oa, &us, path);
    st = ZwCreateFile(&h, DELETE | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT |
        FILE_OPEN_REPARSE_POINT, NULL, 0);
    if (!NT_SUCCESS(st)) {
        if (st == STATUS_ACCESS_DENIED) {
            NTSTATUS c = KcClearReadOnly(path);
            if (NT_SUCCESS(c)) {
                KcInitOa(&oa, &us, path);
                st = ZwCreateFile(&h, DELETE | SYNCHRONIZE, &oa, &ios, NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    FILE_OPEN, FILE_NON_DIRECTORY_FILE |
                    FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT,
                    NULL, 0);
            }
        }
        if (!NT_SUCCESS(st)) return st;
    }
    disp.DeleteFile = TRUE;
    st = g.Apis.ZwSetInformationFile(h, &ios, &disp, sizeof(disp),
        (ULONG)FileDispositionInformation);
    ZwClose(h);
    return st;
}

static NTSTATUS KcDeleteOneEx(const WCHAR* path, BOOLEAN openReparse)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE h = NULL;
    NTSTATUS st;
    ULONG opts = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT |
        FILE_OPEN_REPARSE_POINT;
    KC_FILE_DISPOSITION disp = { 0 };
    UNREFERENCED_PARAMETER(openReparse);
    if (!g.Apis.ZwSetInformationFile) return STATUS_NOT_SUPPORTED;
    KcInitOa(&oa, &us, path);
    st = ZwCreateFile(&h, DELETE | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, opts, NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    disp.DeleteFile = TRUE;
    st = g.Apis.ZwSetInformationFile(h, &ios, &disp, sizeof(disp),
        (ULONG)FileDispositionInformation);
    ZwClose(h);
    return st;
}

static NTSTATUS KcDeleteDirEx(const WCHAR* path, BOOLEAN openReparse)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE h = NULL;
    NTSTATUS st;
    ULONG opts = FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT;
    KC_FILE_DISPOSITION disp = { 0 };
    if (!g.Apis.ZwSetInformationFile) return STATUS_NOT_SUPPORTED;
    if (openReparse) opts |= FILE_OPEN_REPARSE_POINT;
    KcInitOa(&oa, &us, path);
    st = ZwCreateFile(&h, DELETE | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, opts, NULL, 0);
    if (!NT_SUCCESS(st)) return st;
    disp.DeleteFile = TRUE;
    st = g.Apis.ZwSetInformationFile(h, &ios, &disp, sizeof(disp),
        (ULONG)FileDispositionInformation);
    ZwClose(h);
    return st;
}

static NTSTATUS KcDel(const WCHAR* path, kc_u32* outStatus)
{
    NTSTATUS st = KcDeleteOne(path);
    *outStatus = KcNtToKc(st);
    return st;
}

static NTSTATUS KcCopy(const WCHAR* src, const WCHAR* dst, kc_u64* bytes)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE hs = NULL, hd = NULL;
    PVOID buf = NULL;
    LARGE_INTEGER rOff = { 0 }, wOff = { 0 }, chunkOff = { 0 };
    NTSTATUS st = STATUS_SUCCESS;
    ULONG got = 0, written = 0;
    kc_u64 total = 0;

    *bytes = 0;
    buf = KcAlloc(KC_COPY_CHUNK);
    if (!buf) return STATUS_INSUFFICIENT_RESOURCES;

    KcInitOa(&oa, &us, src);
    st = ZwCreateFile(&hs, FILE_READ_DATA | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(st)) goto done;

    KcInitOa(&oa, &us, dst);
    st = ZwCreateFile(&hd, FILE_WRITE_DATA | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL, 0, FILE_OVERWRITE_IF,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(st)) goto done;

    for (;;) {
        st = ZwReadFile(hs, NULL, NULL, NULL, &ios, buf, KC_COPY_CHUNK, &rOff, NULL);
        if (st == STATUS_END_OF_FILE) { st = STATUS_SUCCESS; break; }
        if (!NT_SUCCESS(st)) break;
        got = (ULONG)ios.Information;
        if (got == 0) break;

        written = 0;
        while (written < got) {
            chunkOff.QuadPart = wOff.QuadPart + (LONGLONG)written;
            st = ZwWriteFile(hd, NULL, NULL, NULL, &ios,
                (PUCHAR)buf + written, got - written, &chunkOff, NULL);
            if (!NT_SUCCESS(st)) break;
            if (ios.Information == 0) { st = STATUS_UNSUCCESSFUL; break; }
            written += (ULONG)ios.Information;
        }
        if (!NT_SUCCESS(st)) break;
        rOff.QuadPart += (LONGLONG)got;
        wOff.QuadPart += (LONGLONG)got;
        total += (kc_u64)got;
    }
    *bytes = total;

done:
    if (hd) ZwClose(hd);
    if (hs) ZwClose(hs);
    KcFree(buf);
    return st;
}

static BOOLEAN KcJoin(WCHAR* dst, ULONG cap, const WCHAR* parent,
    const WCHAR* child, ULONG childLen)
{
    ULONG p = KcWcsLen(parent), i;
    if ((SIZE_T)p + 1 + childLen + 1 >= cap) return FALSE;
    for (i = 0; i < p; ++i) dst[i] = parent[i];
    if (p == 0 || dst[p - 1] != L'\\') dst[p++] = L'\\';
    for (i = 0; i < childLen; ++i) dst[p + i] = child[i];
    dst[p + childLen] = L'\0';
    return TRUE;
}

typedef struct _KC_RD_NODE {
    WCHAR   Path[KC_MAX_PATH];
    BOOLEAN IsDir;
    BOOLEAN IsReparse;
} KC_RD_NODE;

typedef struct _KC_RD_PLAN {
    KC_RD_NODE* Nodes;
    ULONG       Count;
    ULONG       Cap;
} KC_RD_PLAN;

typedef struct _KC_RD_QITEM {
    WCHAR   Path[KC_MAX_PATH];
    BOOLEAN IsReparse;
} KC_RD_QITEM;

static NTSTATUS KcPlanPush(KC_RD_PLAN* p, const WCHAR* path, BOOLEAN isDir, BOOLEAN rp)
{
    ULONG i;
    NTSTATUS st;
    if (p->Count >= (ULONG)KC_RD_MAX_NODES) return STATUS_INSUFFICIENT_RESOURCES;
    st = KcGrowArray((PVOID*)&p->Nodes, &p->Cap, p->Count + 1, sizeof(KC_RD_NODE));
    if (!NT_SUCCESS(st)) return st;
    RtlZeroMemory(&p->Nodes[p->Count], sizeof(KC_RD_NODE));
    for (i = 0; i < KC_MAX_PATH && path[i]; ++i)
        p->Nodes[p->Count].Path[i] = path[i];
    p->Nodes[p->Count].Path[KC_MAX_PATH - 1] = L'\0';
    p->Nodes[p->Count].IsDir = isDir;
    p->Nodes[p->Count].IsReparse = rp;
    ++p->Count;
    return STATUS_SUCCESS;
}

static NTSTATUS KcQueuePush(KC_RD_QITEM** q, ULONG* cap, ULONG* count, const WCHAR* path)
{
    ULONG i;
    NTSTATUS st;
    if (*count >= (ULONG)KC_RD_MAX_NODES) return STATUS_INSUFFICIENT_RESOURCES;
    st = KcGrowArray((PVOID*)q, cap, *count + 1, sizeof(KC_RD_QITEM));
    if (!NT_SUCCESS(st)) return st;
    RtlZeroMemory(&(*q)[*count], sizeof(KC_RD_QITEM));
    for (i = 0; i < KC_MAX_PATH && path[i]; ++i) (*q)[*count].Path[i] = path[i];
    (*q)[*count].Path[KC_MAX_PATH - 1] = L'\0';
    ++(*count);
    return STATUS_SUCCESS;
}

static NTSTATUS KcRdCollect(const WCHAR* root, KC_RD_PLAN* plan)
{
    KC_RD_QITEM* q = NULL;
    ULONG qCap = 0, qHead = 0, qCount = 0;
    PVOID dirBuf = NULL;
    HANDLE dh = NULL;
    WCHAR cur[KC_MAX_PATH] = { 0 }, child[KC_MAX_PATH] = { 0 };
    ULONG rootAttr = 0, i, used, valid, nameBytes, nameChars;
    SIZE_T v;
    BOOLEAN restart;
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    NTSTATUS st;

    RtlZeroMemory(plan, sizeof(*plan));
    if (KcWcsLen(root) >= KC_MAX_PATH) return STATUS_NAME_TOO_LONG;
    if (!g.Apis.ZwSetInformationFile || !g.Apis.ZwQueryDirectoryFile)
        return STATUS_NOT_SUPPORTED;

    dirBuf = KcAlloc(KC_COPY_CHUNK);
    if (!dirBuf) return STATUS_INSUFFICIENT_RESOURCES;


    st = KcQueryAttributes(root, &rootAttr);
    if (!NT_SUCCESS(st)) goto done;
    if (rootAttr & FILE_ATTRIBUTE_REPARSE_POINT) {
        st = KcPlanPush(plan, root, TRUE, TRUE);
        goto done;
    }
    if (!(rootAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        st = KcPlanPush(plan, root, FALSE, FALSE);
        goto done;
    }

    st = KcPlanPush(plan, root, TRUE, FALSE);
    if (!NT_SUCCESS(st)) goto done;
    st = KcQueuePush(&q, &qCap, &qCount, root);
    if (!NT_SUCCESS(st)) goto done;

    while (qHead < qCount) {
        for (i = 0; i < KC_MAX_PATH && q[qHead].Path[i]; ++i) cur[i] = q[qHead].Path[i];
        cur[KC_MAX_PATH - 1] = L'\0';
        ++qHead;

        KcInitOa(&oa, &us, cur);
        st = ZwCreateFile(&dh, FILE_LIST_DIRECTORY | SYNCHRONIZE, &oa, &ios, NULL,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
            NULL, 0);

        if (!NT_SUCCESS(st)) goto done;

        restart = TRUE;
        for (;;) {
            KC_DIR_ENTRY* e;
            RtlZeroMemory(dirBuf, KC_COPY_CHUNK);
            st = g.Apis.ZwQueryDirectoryFile(dh, NULL, NULL, NULL, &ios, dirBuf,
                KC_COPY_CHUNK, KC_CLASS_DIR_INFORMATION, FALSE, NULL, restart);
            if (!NT_SUCCESS(st)) {
                if (st == STATUS_NO_MORE_FILES) st = STATUS_SUCCESS;
                break;
            }
            v = (SIZE_T)ios.Information;
            if (v == 0) break;
            if (v > (SIZE_T)KC_COPY_CHUNK) { st = STATUS_INTERNAL_ERROR; break; }
            valid = (ULONG)v;
            restart = FALSE;
            used = 0;
            for (;;) {
                if (used + offsetof(KC_DIR_ENTRY, FileName) > valid) break;
                e = (KC_DIR_ENTRY*)((PUCHAR)dirBuf + used);
                nameBytes = e->FileNameLength;
                if ((SIZE_T)nameBytes > valid - used - offsetof(KC_DIR_ENTRY, FileName)) break;
                nameChars = nameBytes / sizeof(WCHAR);
                if (nameChars > 0 && nameChars < KC_MAX_PATH &&
                    !(e->FileName[0] == L'.' &&
                        (nameChars == 1 || (nameChars == 2 && e->FileName[1] == L'.')))) {
                    if (!KcJoin(child, KC_MAX_PATH, cur, e->FileName, nameChars)) {
                        st = STATUS_NAME_TOO_LONG;
                        break;
                    }
                    if (e->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        BOOLEAN rp = (e->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? TRUE : FALSE;
                        st = KcPlanPush(plan, child, TRUE, rp);
                        if (!NT_SUCCESS(st)) break;
                        if (!rp) {
                            st = KcQueuePush(&q, &qCap, &qCount, child);
                            if (!NT_SUCCESS(st)) break;
                        }
                    }
                    else {
                        BOOLEAN rp = (e->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? TRUE : FALSE;
                        st = KcPlanPush(plan, child, FALSE, rp);
                        if (!NT_SUCCESS(st)) break;
                    }
                }
                if (e->NextEntryOffset == 0) break;
                if ((SIZE_T)e->NextEntryOffset > valid - used) break;
                used += (SIZE_T)e->NextEntryOffset;
            }
            if (!NT_SUCCESS(st)) break;
        }
        ZwClose(dh);
        dh = NULL;
        if (!NT_SUCCESS(st)) goto done;
    }

done:
    if (dh) ZwClose(dh);
    KcFree(dirBuf);
    KcFree(q);
    if (!NT_SUCCESS(st) || plan->Count == 0) {
        KcFree(plan->Nodes);
        plan->Nodes = NULL;
        plan->Count = 0;
        plan->Cap = 0;
        if (NT_SUCCESS(st)) st = STATUS_NOT_FOUND;
    }
    return st;
}

static kc_u32 KcRdExecute(KC_RD_PLAN* plan, KC_RD_RESULT* out)
{
    ULONG i;
    NTSTATUS ds;
    kc_u32 first = KC_OK;
    for (i = 0; i < plan->Count; ++i) {
        KC_RD_NODE* n = &plan->Nodes[i];
        if (n->IsDir) continue;
        ds = n->IsReparse ? KcDeleteOneEx(n->Path, TRUE) : KcDeleteOne(n->Path);
        if (NT_SUCCESS(ds)) ++out->DeletedFiles;
        else {
            ++out->Failed;
            if (first == KC_OK) first = KcNtToKc(ds);
        }
    }

    for (i = plan->Count; i > 0; --i) {
        KC_RD_NODE* n = &plan->Nodes[i - 1];
        if (!n->IsDir) continue;
        ds = KcDeleteDirEx(n->Path, n->IsReparse);
        if (NT_SUCCESS(ds)) ++out->DeletedDirs;
        else {
            ++out->Failed;
            if (first == KC_OK) first = KcNtToKc(ds);
        }
    }
    out->Partial = (out->Failed != 0) ? 1u : 0u;
    if (first == (kc_u32)STATUS_ACCESS_DENIED) first = KC_ERR_FP_BLOCKED;
    out->Status = first;
    return first;
}

static NTSTATUS KcRd(const WCHAR* root, KC_RD_RESULT* out)
{
    KC_RD_PLAN plan = { 0 };
    NTSTATUS st;
    RtlZeroMemory(out, sizeof(*out));
    st = KcRdCollect(root, &plan);
    if (!NT_SUCCESS(st)) {
        out->Status = (st == STATUS_INSUFFICIENT_RESOURCES)
            ? KC_ERR_PLAN_FULL : KcNtToKc(st);
        out->Partial = 0u;
        return STATUS_SUCCESS;
    }
    ExAcquireResourceExclusiveLite(&g.File.RdLock, TRUE);
    (VOID)KcRdExecute(&plan, out);
    ExReleaseResourceLite(&g.File.RdLock);
    KcFree(plan.Nodes);
    return STATUS_SUCCESS;
}

static NTSTATUS KcRegQuery(const WCHAR* regPath, const WCHAR* valueName, KC_REG_RESULT* out)
{
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 }, vn = { 0 };
    HANDLE h = NULL;
    NTSTATUS st;
    ULONG need = 0, need2 = 0, cap = 0;
    SIZE_T dataOff, avail;
    KC_KEY_VALUE_PARTIAL* kv = NULL;

    RtlZeroMemory(out, sizeof(*out));
    if (!g.Apis.ZwQueryValueKey) {
        out->Status = KC_ERR_API_MISSING;
        return STATUS_NOT_SUPPORTED;
    }
    KcInitOa(&oa, &us, regPath);
    st = ZwOpenKey(&h, KC_KEY_READ, &oa);
    if (!NT_SUCCESS(st)) { out->Status = KcNtToKc(st); return st; }
    RtlInitUnicodeString(&vn, valueName);
    st = g.Apis.ZwQueryValueKey(h, &vn, KC_CLASS_KEY_VALUE_PARTIAL, NULL, 0, &need);
    if (st != STATUS_BUFFER_TOO_SMALL && st != STATUS_BUFFER_OVERFLOW && !NT_SUCCESS(st)) {
        ZwClose(h); out->Status = KcNtToKc(st); return st;
    }
    if (need == 0 || (SIZE_T)need > (SIZE_T)KC_REG_DATA + sizeof(KC_KEY_VALUE_PARTIAL)) {
        ZwClose(h);
        out->Status = KC_ERR_ALLOC_FAILED;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    cap = need;
    kv = (KC_KEY_VALUE_PARTIAL*)KcAlloc((SIZE_T)cap);
    if (!kv) {
        ZwClose(h);
        out->Status = KC_ERR_ALLOC_FAILED;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    need2 = cap;
    st = g.Apis.ZwQueryValueKey(h, &vn, KC_CLASS_KEY_VALUE_PARTIAL, kv, cap, &need2);
    if (NT_SUCCESS(st)) {
        dataOff = (SIZE_T)FIELD_OFFSET(KC_KEY_VALUE_PARTIAL, Data);
        avail = ((SIZE_T)cap > dataOff) ? (SIZE_T)cap - dataOff : 0;
        if ((SIZE_T)kv->DataLength > avail) st = STATUS_BUFFER_OVERFLOW;
    }
    if (NT_SUCCESS(st)) {
        out->Status = KC_OK;
        out->ValueType = kv->Type;
        out->DataLength = kv->DataLength;
        if (out->DataLength > KC_REG_DATA) out->DataLength = KC_REG_DATA;
        RtlCopyMemory(out->Data, kv->Data, out->DataLength);
    }
    else out->Status = KcNtToKc(st);
    KcFree(kv);
    ZwClose(h);
    return st;
}

static BOOLEAN KcFpSame(const WCHAR* a, ULONG n, const WCHAR* b, ULONG m)
{
    ULONG i;
    if (n != m) return FALSE;
    for (i = 0; i < n; ++i) {
        WCHAR x = a[i], y = b[i];
        if (x >= L'A' && x <= L'Z') x = (WCHAR)(x - L'A' + L'a');
        if (y >= L'A' && y <= L'Z') y = (WCHAR)(y - L'A' + L'a');
        if (x != y) return FALSE;
    }
    return TRUE;
}

static BOOLEAN KcFpPrefix(const WCHAR* name, const WCHAR* rule, ULONG n)
{
    ULONG i;
    WCHAR tail;
    for (i = 0; i < n; ++i) {
        WCHAR x = name[i], y = rule[i];
        if (!x) return FALSE;
        if (x >= L'A' && x <= L'Z') x = (WCHAR)(x - L'A' + L'a');
        if (y >= L'A' && y <= L'Z') y = (WCHAR)(y - L'A' + L'a');
        if (x != y) return FALSE;
    }
    tail = name[n];
    return tail == L'\0' || tail == L'\\';
}

static BOOLEAN KcFpHit(const WCHAR* name)
{
    KIRQL irql;
    BOOLEAN hit = FALSE;
    ULONG i;
    KeAcquireSpinLock(&g.Fp.Lock, &irql);
    for (i = 0; i < (ULONG)KC_FP_MAX; ++i) {
        if (g.Fp.RuleActive[i] && g.Fp.RuleLen[i] &&
            KcFpPrefix(name, g.Fp.Rules[i], g.Fp.RuleLen[i])) {
            hit = TRUE;
            break;
        }
    }
    KeReleaseSpinLock(&g.Fp.Lock, irql);
    return hit;
}

static kc_u32 KcFpApply(const WCHAR* path, ULONG op, KC_FP_RESULT* out)
{
    KIRQL irql;
    ULONG i, n = 0, slot = (ULONG)KC_FP_MAX;
    BOOLEAN found = FALSE;
    kc_u32 s = KC_OK;

    RtlZeroMemory(out, sizeof(*out));
    if (op == KC_FP_ADD || op == KC_FP_REMOVE) {
        n = KcWcsLen(path);
        while (n > 1 && path[n - 1] == L'\\') --n;
    }
    KeAcquireSpinLock(&g.Fp.Lock, &irql);
    if (op == KC_FP_CLEAR) {
        for (i = 0; i < (ULONG)KC_FP_MAX; ++i) {
            g.Fp.RuleActive[i] = FALSE;
            g.Fp.RuleLen[i] = 0;
            g.Fp.Rules[i][0] = L'\0';
        }
        InterlockedExchange(&g.Fp.RuleCount, 0);
    }
    else if (op == KC_FP_ADD) {
        for (i = 0; i < (ULONG)KC_FP_MAX; ++i) {
            if (!g.Fp.RuleActive[i]) { if (slot == (ULONG)KC_FP_MAX) slot = i; continue; }
            if (KcFpSame(path, n, g.Fp.Rules[i], g.Fp.RuleLen[i])) { found = TRUE; break; }
        }
        if (!found) {
            if (slot == (ULONG)KC_FP_MAX) s = KC_ERR_FP_FULL;
            else if (n == 0 || n >= KC_MAX_PATH) s = KC_ERR_PATH_LONG;
            else {
                for (i = 0; i < n; ++i) g.Fp.Rules[slot][i] = path[i];
                g.Fp.Rules[slot][n] = L'\0';
                g.Fp.RuleLen[slot] = n;
                g.Fp.RuleActive[slot] = TRUE;
                InterlockedIncrement(&g.Fp.RuleCount);
            }
        }
    }
    else if (op == KC_FP_REMOVE) {
        for (i = 0; i < (ULONG)KC_FP_MAX; ++i) {
            if (g.Fp.RuleActive[i] && KcFpSame(path, n, g.Fp.Rules[i], g.Fp.RuleLen[i])) {
                g.Fp.RuleActive[i] = FALSE;
                g.Fp.RuleLen[i] = 0;
                g.Fp.Rules[i][0] = L'\0';
                InterlockedDecrement(&g.Fp.RuleCount);
                found = TRUE;
                break;
            }
        }
        if (!found) s = KC_ERR_FP_NOT_FOUND;
    }
    out->Count = (kc_u32)g.Fp.RuleCount;
    out->Blocked = (kc_u32)g.Fp.Blocked;
    out->Observed = (kc_u32)g.Fp.Observed;
    for (i = 0; i < (ULONG)KC_FP_MAX; ++i)
        if (g.Fp.RuleActive[i]) (VOID)KcPack(out->Rules[i], KC_MAX_PATH, g.Fp.Rules[i]);
    KeReleaseSpinLock(&g.Fp.Lock, irql);
    out->Status = s;
    return s;
}

static BOOLEAN KcFpIsDelete(const PFLT_CALLBACK_DATA Data)
{
    UCHAR mj = Data->Iopb->MajorFunction;
    ULONG cls, flags = 0, len;
    PVOID buf;
    BOOLEAN del = FALSE, fault = FALSE;
    if (mj == IRP_MJ_CREATE)
        return (Data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE) ? TRUE : FALSE;
    if (mj != IRP_MJ_SET_INFORMATION) return FALSE;
    cls = (ULONG)Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
    buf = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
    len = Data->Iopb->Parameters.SetFileInformation.Length;
    if (cls == (ULONG)FileDispositionInformation) {
        if (!buf || len < sizeof(BOOLEAN)) return TRUE;
        __try { RtlCopyMemory(&del, buf, sizeof(del)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { fault = TRUE; }
        return fault ? TRUE : (del ? TRUE : FALSE);
    }
    if (cls == (ULONG)FileDispositionInformationEx) {
        if (!buf || len < sizeof(ULONG)) return TRUE;
        __try { RtlCopyMemory(&flags, buf, sizeof(flags)); }
        __except (EXCEPTION_EXECUTE_HANDLER) { fault = TRUE; }
        return fault ? TRUE : ((flags & KC_FP_DISPOSITION_DELETE) ? TRUE : FALSE);
    }
    return FALSE;
}

static FLT_PREOP_CALLBACK_STATUS
KcFltPre(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext)
{
    UCHAR mj;
    WCHAR detail[KC_DETAIL] = { 0 }, name[KC_MAX_PATH] = { 0 };
    PFLT_FILE_NAME_INFORMATION ni = NULL;
    FLT_FILE_NAME_OPTIONS opts;
    NTSTATUS st;
    ULONG n, i, full;
    BOOLEAN rules, observing;
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    if (!Data || !Data->Iopb) return FLT_PREOP_SUCCESS_NO_CALLBACK;
    mj = Data->Iopb->MajorFunction;
    if (mj < KC_FLT_OPS) InterlockedIncrement(&g.Flt.Ops[mj]);
    if (mj != IRP_MJ_CREATE && mj != IRP_MJ_SET_INFORMATION && mj != IRP_MJ_CLEANUP)
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    rules = (g.Fp.RuleCount != 0);
    observing = ((g.Cb.Mask & KC_CB_FILE) != 0);
    if (!observing && !rules) return FLT_PREOP_SUCCESS_NO_CALLBACK;

    if (KeGetCurrentIrql() <= APC_LEVEL) {

        opts = FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT;
        st = FltGetFileNameInformation(Data, opts, &ni);
        if (NT_SUCCESS(st) && ni && ni->Name.Buffer && ni->Name.Length) {
            full = (ULONG)((SIZE_T)ni->Name.Length / sizeof(WCHAR));
            if (full >= KC_MAX_PATH) full = KC_MAX_PATH - 1;
            for (i = 0; i < full; ++i) name[i] = ni->Name.Buffer[i];
            name[full] = L'\0';
            n = full < KC_DETAIL ? full : KC_DETAIL - 1;
            for (i = 0; i < n; ++i) detail[i] = ni->Name.Buffer[i];
            detail[n] = L'\0';
            FltReleaseFileNameInformation(ni);
        }
        else if (ni) FltReleaseFileNameInformation(ni);
    }
    if (detail[0] == L'\0') {
        const WCHAR* tag = (mj == IRP_MJ_CREATE) ? L"CREATE"
            : (mj == IRP_MJ_SET_INFORMATION) ? L"SET_INFORMATION" : L"CLEANUP";
        for (i = 0; i < KC_DETAIL - 1 && tag[i]; ++i) detail[i] = tag[i];
        detail[i] = L'\0';
    }

    if (rules && KcFpIsDelete(Data)) {
        if (name[0]) {
            if (KcFpHit(name)) {
                InterlockedIncrement(&g.Fp.Blocked);
                KcEmitEvent(KC_EVT_FILE, (ULONG)(ULONG_PTR)PsGetCurrentProcessId(),
                    (ULONG)mj, STATUS_ACCESS_DENIED, detail);
                Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                Data->IoStatus.Information = 0;
                return FLT_PREOP_COMPLETE;
            }
        }
        else if (mj == IRP_MJ_CREATE) {
            InterlockedIncrement(&g.Fp.Blocked);
            KcEmitEvent(KC_EVT_FILE, (ULONG)(ULONG_PTR)PsGetCurrentProcessId(),
                (ULONG)mj, STATUS_ACCESS_DENIED, detail);
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            Data->IoStatus.Information = 0;
            return FLT_PREOP_COMPLETE;
        }
        else {
            KcEmitEvent(KC_EVT_FILE, (ULONG)(ULONG_PTR)PsGetCurrentProcessId(),
                (ULONG)mj, STATUS_SUCCESS, detail);
        }
    }
    if (observing) {
        InterlockedIncrement(&g.Fp.Observed);
        KcEmitEvent(KC_EVT_FILE, (ULONG)(ULONG_PTR)PsGetCurrentProcessId(),
            (ULONG)mj, STATUS_SUCCESS, detail);
    }
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

static NTSTATUS
KcInstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags,
    DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);
    InterlockedIncrement(&g.Flt.InstanceCount);
    return STATUS_SUCCESS;
}

static VOID KcInstanceTeardownComplete(PCFLT_RELATED_OBJECTS FltObjects,
    FLT_INSTANCE_TEARDOWN_FLAGS Reason)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Reason);
    InterlockedDecrement(&g.Flt.InstanceCount);
}

static NTSTATUS KcFltUnload(FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    return STATUS_FLT_DO_NOT_DETACH;
}

static const FLT_OPERATION_REGISTRATION KcFltOps[] = {
    { IRP_MJ_CREATE,          0, KcFltPre, NULL, NULL },
    { IRP_MJ_READ,            0, KcFltPre, NULL, NULL },
    { IRP_MJ_WRITE,           0, KcFltPre, NULL, NULL },
    { IRP_MJ_SET_INFORMATION, 0, KcFltPre, NULL, NULL },
    { IRP_MJ_CLEANUP,         0, KcFltPre, NULL, NULL },
    { IRP_MJ_OPERATION_END,   0, NULL,      NULL, NULL }
};

static NTSTATUS KcFltInit(PDRIVER_OBJECT Drv)
{
    FLT_REGISTRATION reg = { 0 };
    NTSTATUS st;
    reg.Size = (USHORT)sizeof(reg);
    reg.Version = FLT_REGISTRATION_VERSION;
    reg.OperationRegistration = KcFltOps;
    reg.InstanceSetupCallback = KcInstanceSetup;
    reg.InstanceTeardownCompleteCallback = KcInstanceTeardownComplete;
    reg.FilterUnloadCallback = KcFltUnload;
    st = FltRegisterFilter(Drv, &reg, &g.Flt.Filter);
    if (!NT_SUCCESS(st)) {
        g.Flt.Filter = NULL;
        g.Flt.State = KC_FLT_FAILED;
        g.Flt.LastStatus = (kc_u32)st;
        return st;
    }
    g.Flt.State = KC_FLT_REGISTERED;
    st = FltStartFiltering(g.Flt.Filter);
    if (NT_SUCCESS(st)) {
        g.Flt.Started = TRUE;
        g.Flt.State = KC_FLT_STARTED;
        g.Flt.LastStatus = (kc_u32)STATUS_SUCCESS;
    }
    else {
        g.Flt.State = KC_FLT_FAILED;
        g.Flt.LastStatus = (kc_u32)st;
    }

    if (NT_SUCCESS(st)) {
        NTSTATUS ps = KcPortInit();
        if (!NT_SUCCESS(ps))
            DbgPrint("[Kernel_Tools] event port unavailable: 0x%08X\n", (ULONG)ps);
    }
    return st;
}

static NTSTATUS KcFltReady(VOID)
{
    if (!g.Flt.Filter || !g.Flt.Started || g.Flt.InstanceCount == 0) {
        g.Flt.LastStatus = (kc_u32)STATUS_NOT_SUPPORTED;
        return STATUS_NOT_SUPPORTED;
    }
    return STATUS_SUCCESS;
}

static VOID KcFltStop(VOID)
{
    if (g.Flt.Filter) {
        FltUnregisterFilter(g.Flt.Filter);
        g.Flt.Filter = NULL;
    }
    g.Flt.Started = FALSE;
    g.Flt.State = KC_FLT_UNINIT;
}

static VOID KcPsNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    WCHAR detail[KC_DETAIL] = { 0 };
    ULONG i, n = 0;
    UNREFERENCED_PARAMETER(Process);
    if (CreateInfo && CreateInfo->ImageFileName && CreateInfo->ImageFileName->Buffer) {
        n = (ULONG)((SIZE_T)CreateInfo->ImageFileName->Length / sizeof(WCHAR));
        if (n >= KC_DETAIL) n = KC_DETAIL - 1;
        for (i = 0; i < n; ++i) detail[i] = CreateInfo->ImageFileName->Buffer[i];
        detail[i] = L'\0';
    }
    KcEmitEvent(KC_EVT_PROC, (ULONG)(ULONG_PTR)ProcessId,
        CreateInfo ? 1u : 0u, STATUS_SUCCESS, detail);
}

static NTSTATUS KcCmCallback(PVOID Ctx, PVOID Arg1, PVOID Arg2)
{
    ULONG cls = (ULONG)(ULONG_PTR)Arg1;
    UNREFERENCED_PARAMETER(Ctx);
    UNREFERENCED_PARAMETER(Arg2);
    if (g.Cb.Mask & KC_CB_REG)
        KcEmitEvent(KC_EVT_REG, 0, cls, STATUS_SUCCESS, L"Registry");
    return STATUS_SUCCESS;
}

static OB_PREOP_CALLBACK_STATUS KcObPre(PVOID Ctx, POB_PRE_OPERATION_INFORMATION Info)
{
    UNREFERENCED_PARAMETER(Ctx);
    if ((g.Cb.Mask & KC_CB_HANDLE) && Info && Info->ObjectType == *PsProcessType) {
        KcEmitEvent(KC_EVT_HANDLE, (ULONG)(ULONG_PTR)PsGetCurrentProcessId(),
            (ULONG)Info->Operation, STATUS_SUCCESS, L"Handle");
    }
    return OB_PREOP_SUCCESS;
}

static NTSTATUS KcCallbacksEnable(ULONG mask)
{
    NTSTATUS st = STATUS_SUCCESS, first = STATUS_SUCCESS;
    OB_CALLBACK_REGISTRATION reg = { 0 };
    OB_OPERATION_REGISTRATION op = { 0 };
    ULONG todo;
    ExAcquireResourceExclusiveLite(&g.Cb.Lock, TRUE);
    todo = mask & ~g.Cb.Mask;

    if (todo & KC_CB_PROC) {
        st = PsSetCreateProcessNotifyRoutineEx(KcPsNotify, FALSE);
        if (NT_SUCCESS(st)) g.Cb.Mask |= KC_CB_PROC;
        else if (NT_SUCCESS(first)) first = st;
    }
    if (todo & KC_CB_REG) {
        st = CmRegisterCallback(KcCmCallback, NULL, &g.Cb.CmCookie);
        if (NT_SUCCESS(st)) g.Cb.Mask |= KC_CB_REG;
        else if (NT_SUCCESS(first)) first = st;
    }
    if (todo & KC_CB_HANDLE) {
        op.ObjectType = PsProcessType;
        op.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
        op.PreOperation = KcObPre;
        reg.Version = OB_FLT_REGISTRATION_VERSION;
        reg.OperationRegistrationCount = 1;
        reg.OperationRegistration = &op;
        RtlInitUnicodeString(&reg.Altitude, L"399999");
        st = ObRegisterCallbacks(&reg, &g.Cb.ObHandle);
        if (NT_SUCCESS(st)) g.Cb.Mask |= KC_CB_HANDLE;
        else { g.Cb.ObHandle = NULL; if (NT_SUCCESS(first)) first = st; }
    }
    if (todo & KC_CB_FILE) {
        st = KcFltReady();
        if (NT_SUCCESS(st)) g.Cb.Mask |= KC_CB_FILE;
        else if (NT_SUCCESS(first)) first = st;
    }
    ++g.Cb.Generation;
    ExReleaseResourceLite(&g.Cb.Lock);
    return first;
}

static VOID KcCallbacksDisable(ULONG mask)
{
    ExAcquireResourceExclusiveLite(&g.Cb.Lock, TRUE);
    if ((mask & KC_CB_FILE) && (g.Cb.Mask & KC_CB_FILE))
        g.Cb.Mask &= ~KC_CB_FILE;
    if ((mask & KC_CB_HANDLE) && (g.Cb.Mask & KC_CB_HANDLE)) {
        if (g.Cb.ObHandle) ObUnRegisterCallbacks(g.Cb.ObHandle);
        g.Cb.ObHandle = NULL;
        g.Cb.Mask &= ~KC_CB_HANDLE;
    }
    if ((mask & KC_CB_REG) && (g.Cb.Mask & KC_CB_REG)) {
        CmUnRegisterCallback(g.Cb.CmCookie);
        RtlZeroMemory(&g.Cb.CmCookie, sizeof(g.Cb.CmCookie));
        g.Cb.Mask &= ~KC_CB_REG;
    }
    if ((mask & KC_CB_PROC) && (g.Cb.Mask & KC_CB_PROC)) {
        PsSetCreateProcessNotifyRoutineEx(KcPsNotify, TRUE);
        g.Cb.Mask &= ~KC_CB_PROC;
    }
    ++g.Cb.Generation;
    ExReleaseResourceLite(&g.Cb.Lock);
}

static VOID KcCallbacksDisableAll(VOID)
{
    KcCallbacksDisable(KC_CB_PROC | KC_CB_REG | KC_CB_HANDLE | KC_CB_FILE);
}

static VOID KcFreeSnap(ULONG slot)
{
    if (slot >= (ULONG)KC_ENUM_SLOTS) return;
    if (g.Enum.S[slot].Snap) {
        KcFree(g.Enum.S[slot].Snap);
        g.Enum.S[slot].Snap = NULL;
    }
    g.Enum.S[slot].Count = 0;
    g.Enum.S[slot].Session = 0;
}

static NTSTATUS KcBuildProcSnap(KC_ENUM_ITEM** out, ULONG* count)
{
    KC_ENUM_ITEM* arr = NULL;
    ULONG cap = 0, n = 0, pid, i;
    NTSTATUS st;
    *out = NULL; *count = 0;
    if (!g.Apis.PsLookupProcessByProcessId) return STATUS_NOT_SUPPORTED;
    for (pid = 4; pid < (ULONG)KC_PROC_PID_MAX; ++pid) {
        PEPROCESS p = NULL;
        UCHAR prot = 0;
        if (!NT_SUCCESS(g.Apis.PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &p))) continue;
        st = KcGrowArray((PVOID*)&arr, &cap, n + 1, sizeof(KC_ENUM_ITEM));
        if (!NT_SUCCESS(st)) { ObDereferenceObject(p); KcFree(arr); return st; }
        RtlZeroMemory(&arr[n], sizeof(arr[n]));
        arr[n].Id = pid;
        if (g.Apis.PsGetProcessProtection) prot = g.Apis.PsGetProcessProtection(p);
        arr[n].Flags = (kc_u32)prot;
        if (g.Apis.PsGetProcessImageFileName) {
            PCHAR ansi = (PCHAR)g.Apis.PsGetProcessImageFileName(p);
            if (ansi) {
                for (i = 0; i + 1 < (ULONG)KC_ENUM_NAME && ansi[i]; ++i)
                    arr[n].Name[i] = (kc_u16)(UCHAR)ansi[i];
            }
        }
        ObDereferenceObject(p);
        ++n;
    }
    *out = arr; *count = n;
    return STATUS_SUCCESS;
}

static NTSTATUS KcBuildDirSnap(const WCHAR* path, KC_ENUM_ITEM** out, ULONG* count)
{
    KC_ENUM_ITEM* arr = NULL;
    ULONG cap = 0, n = 0, used = 0, i;
    SIZE_T valid;
    OBJECT_ATTRIBUTES oa = { 0 };
    UNICODE_STRING us = { 0 };
    IO_STATUS_BLOCK ios = { 0 };
    HANDLE dh = NULL;
    BOOLEAN restart = TRUE;
    PVOID buf = NULL;
    NTSTATUS st;
    *out = NULL; *count = 0;
    if (!g.Apis.ZwQueryDirectoryFile) return STATUS_NOT_SUPPORTED;
    buf = KcAlloc(KC_COPY_CHUNK);
    if (!buf) return STATUS_INSUFFICIENT_RESOURCES;
    KcInitOa(&oa, &us, path);
    st = ZwCreateFile(&dh, FILE_LIST_DIRECTORY | SYNCHRONIZE, &oa, &ios, NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!NT_SUCCESS(st)) { KcFree(buf); return st; }
    for (;;) {
        KC_DIR_ENTRY* e;
        ULONG nameBytes, nameChars;
        RtlZeroMemory(buf, KC_COPY_CHUNK);
        st = g.Apis.ZwQueryDirectoryFile(dh, NULL, NULL, NULL, &ios, buf,
            KC_COPY_CHUNK, KC_CLASS_DIR_INFORMATION, FALSE, NULL, restart);
        if (!NT_SUCCESS(st)) {
            if (st == STATUS_NO_MORE_FILES) st = STATUS_SUCCESS;
            break;
        }
        valid = (SIZE_T)ios.Information;
        if (valid == 0) break;
        if (valid > (SIZE_T)KC_COPY_CHUNK) { st = STATUS_INTERNAL_ERROR; break; }
        restart = FALSE;
        used = 0;
        for (;;) {
            if (used + offsetof(KC_DIR_ENTRY, FileName) > valid) break;
            e = (KC_DIR_ENTRY*)((PUCHAR)buf + used);
            nameBytes = e->FileNameLength;
            if ((SIZE_T)nameBytes > valid - used - offsetof(KC_DIR_ENTRY, FileName)) break;
            nameChars = nameBytes / sizeof(WCHAR);
            if (nameChars > 0 && nameChars < (ULONG)KC_ENUM_NAME &&
                !(e->FileName[0] == L'.' &&
                    (nameChars == 1 || (nameChars == 2 && e->FileName[1] == L'.')))) {
                NTSTATUS gs = KcGrowArray((PVOID*)&arr, &cap, n + 1, sizeof(KC_ENUM_ITEM));
                if (!NT_SUCCESS(gs)) { st = gs; goto done; }
                RtlZeroMemory(&arr[n], sizeof(arr[n]));
                arr[n].Id = n;
                arr[n].Flags = e->FileAttributes;
                arr[n].Size = (kc_u64)e->EndOfFile.QuadPart;
                for (i = 0; i < nameChars; ++i) arr[n].Name[i] = (kc_u16)e->FileName[i];
                arr[n].Name[nameChars] = 0;
                ++n;
            }
            if (e->NextEntryOffset == 0) break;
            if ((SIZE_T)e->NextEntryOffset > valid - used) break;
            used += (SIZE_T)e->NextEntryOffset;
        }
    }
done:
    ZwClose(dh);
    KcFree(buf);
    if (!NT_SUCCESS(st)) { if (arr) KcFree(arr); return st; }
    *out = arr; *count = n;
    return STATUS_SUCCESS;
}

static NTSTATUS KcVerifySnapLiveness(const KC_ENUM_ITEM* items, ULONG count)
{
    ULONG i;
    for (i = 0; i < count; ++i) {
        PEPROCESS p = NULL;
        if (!NT_SUCCESS(g.Apis.PsLookupProcessByProcessId(
            (HANDLE)(ULONG_PTR)items[i].Id, &p)))
            return STATUS_OBJECT_NAME_NOT_FOUND;
        ObDereferenceObject(p);
    }
    return STATUS_SUCCESS;
}

static kc_u32 KcEnumOpen(ULONG kind, const WCHAR* path, ULONG ownerPid,
    kc_u32 flags, KC_ENUM_OPEN_RESULT* out)
{
    ULONG i, slot, sess = 0;
    KC_ENUM_SESSION* s;
    KC_ENUM_ITEM* items = NULL;
    ULONG count = 0;
    NTSTATUS st;
    kc_u32 r = KC_OK;

    RtlZeroMemory(out, sizeof(*out));
    out->Kind = kind;
    if (kind != KC_ENUM_KIND_PROC && kind != KC_ENUM_KIND_DIR) return KC_ERR_BAD_INPUT;

    ExAcquireResourceExclusiveLite(&g.Enum.Lock, TRUE);
    slot = (ULONG)KC_ENUM_SLOTS;
    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
        if (!g.Enum.S[i].Open && !g.Enum.S[i].Building) { slot = i; break; }
    }
    if (slot == (ULONG)KC_ENUM_SLOTS) {
        ULONG best = 0xffffffffu, oldest = 0, age;
        for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) {
            age = g.Enum.Tick - g.Enum.S[i].LastUse;
            if (age < best) { best = age; oldest = i; }
        }
        slot = oldest;
        KcFreeSnap(slot);
        g.Enum.S[slot].Open = FALSE;
        if (g.Enum.OpenCount) --g.Enum.OpenCount;
    }
    ++g.Enum.Epoch;
    if (g.Enum.Epoch == 0) g.Enum.Epoch = 1;
    sess = (g.Enum.Epoch << 4) | (slot + 1);
    s = &g.Enum.S[slot];
    s->Open = TRUE;
    s->Building = TRUE;
    s->OwnerPid = ownerPid;
    s->Kind = kind;
    s->Session = sess;
    s->LastUse = ++g.Enum.Tick;
    s->Snap = NULL;
    s->Count = 0;
    ++g.Enum.OpenCount;
    ExReleaseResourceLite(&g.Enum.Lock);


    KcGateEnterSnapshot();
    st = (kind == KC_ENUM_KIND_PROC)
        ? KcBuildProcSnap(&items, &count)
        : KcBuildDirSnap(path, &items, &count);
    KcGateLeaveSnapshot();
    if (NT_SUCCESS(st) && (flags & KC_ENUM_STRICT_LIVENESS))
        st = KcVerifySnapLiveness(items, count);

    if (st == STATUS_OBJECT_NAME_NOT_FOUND) {
        if (items) KcFree(items);
        r = KC_ERR_GONE;
        goto fail;
    }
    if (!NT_SUCCESS(st)) {
        if (items) KcFree(items);
        r = KcNtToKc(st);
        goto fail;
    }

    ExAcquireResourceExclusiveLite(&g.Enum.Lock, TRUE);
    if (!g.Enum.S[slot].Open || g.Enum.S[slot].Session != sess) {
        ExReleaseResourceLite(&g.Enum.Lock);
        KcFree(items);
        r = KC_ERR_SESSION_CLOSED;
        goto done;
    }
    g.Enum.S[slot].Snap = items;
    g.Enum.S[slot].Count = count;
    g.Enum.S[slot].Building = FALSE;
    ExReleaseResourceLite(&g.Enum.Lock);
    out->Status = KC_OK;
    out->Session = sess;
    out->Total = count;
    return KC_OK;

fail:
    ExAcquireResourceExclusiveLite(&g.Enum.Lock, TRUE);
    if (g.Enum.S[slot].Session == sess) {
        KcFreeSnap(slot);
        g.Enum.S[slot].Open = FALSE;
        g.Enum.S[slot].Building = FALSE;
        if (g.Enum.OpenCount) --g.Enum.OpenCount;
    }
    ExReleaseResourceLite(&g.Enum.Lock);
done:
    out->Status = r;
    return r;
}

static kc_u32 KcEnumPage(ULONG session, ULONG cursor, ULONG kind,
    ULONG ownerPid, KC_ENUM_PAGE* page)
{
    ULONG slot, i, n, total;
    KC_ENUM_SESSION* s;
    kc_u32 r;
    RtlZeroMemory(page, sizeof(*page));
    slot = session & 0xF;
    if (slot == 0 || slot > (ULONG)KC_ENUM_SLOTS) {
        page->Status = KC_ERR_ENUM_INVALID;
        return KC_ERR_ENUM_INVALID;
    }
    --slot;
    ExAcquireResourceExclusiveLite(&g.Enum.Lock, TRUE);
    s = &g.Enum.S[slot];
    if (!s->Open || s->Session != session) r = KC_ERR_SESSION_CLOSED;
    else if (s->OwnerPid != ownerPid) r = KC_ERR_NOT_OWNER;
    else if (kind != 0 && s->Kind != kind) r = KC_ERR_ENUM_INVALID;
    else if (s->Building) r = KC_ERR_BUSY;
    else if (cursor > s->Count || (!s->Snap && s->Count != 0)) r = KC_ERR_ENUM_INVALID;
    else {
        total = s->Count;
        n = total - cursor;
        if (n > (ULONG)KC_ENUM_ITEM_MAX) n = (ULONG)KC_ENUM_ITEM_MAX;
        for (i = 0; i < n; ++i) page->Items[i] = s->Snap[cursor + i];
        s->LastUse = ++g.Enum.Tick;
        page->Status = KC_OK;
        page->Session = session;
        page->Total = total;
        page->BatchNo = cursor / (ULONG)KC_ENUM_ITEM_MAX;
        page->Count = n;
        page->More = (cursor + n < total) ? 1u : 0u;
        r = KC_OK;
    }
    ExReleaseResourceLite(&g.Enum.Lock);
    if (r != KC_OK) page->Status = r;
    return r;
}

static kc_u32 KcEnumClose(ULONG session, ULONG ownerPid)
{
    ULONG slot;
    KC_ENUM_SESSION* s;
    kc_u32 r;
    slot = session & 0xF;
    if (slot == 0 || slot > (ULONG)KC_ENUM_SLOTS) return KC_ERR_ENUM_INVALID;
    --slot;
    ExAcquireResourceExclusiveLite(&g.Enum.Lock, TRUE);
    s = &g.Enum.S[slot];
    if (!s->Open || s->Session != session) r = KC_ERR_SESSION_CLOSED;
    else if (s->OwnerPid != ownerPid) r = KC_ERR_NOT_OWNER;
    else {
        KcFreeSnap(slot);
        s->Open = FALSE;
        s->Building = FALSE;
        if (g.Enum.OpenCount) --g.Enum.OpenCount;
        r = KC_OK;
    }
    ExReleaseResourceLite(&g.Enum.Lock);
    return r;
}

typedef NTSTATUS(*KC_HANDLER)(const KC_REQUEST* Req, KC_REPLY* Rep);

static NTSTATUS H_Ping(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    UNREFERENCED_PARAMETER(Req);
    Rep->Status = KC_OK;
    Rep->Bytes = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS H_Status(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_STATUS_RESULT r = { 0 };
    ULONG i, slots = 0;
    KIRQL irql;
    UNREFERENCED_PARAMETER(Req);
    r.Status = KC_OK;
    r.Capabilities = 0x1Fu;
    r.Lifecycle = (kc_u32)g.Lifecycle.State;
    r.ActiveRequests = (kc_u32)g.Lifecycle.ActiveRequests;
    r.BuildNumber = g.Lifecycle.BuildNumber;
    r.LegacyDropped = (kc_u32)g.Legacy.Dropped;
    KeAcquireSpinLock(&g.Fanout.Lock, &irql);
    for (i = 0; i < (ULONG)KC_ENUM_SLOTS; ++i) if (g.Fanout.Owner[i]) ++slots;
    KeReleaseSpinLock(&g.Fanout.Lock, irql);
    r.FanoutSlots = slots;
    r.EnumOpen = g.Enum.OpenCount;
    r.CallbackMask = g.Cb.Mask;
    r.FltState = g.Flt.State;
    Rep->Status = KC_OK;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_ResolvePath(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW v = { 0 };
    KC_PATH_RESULT r = { 0 };
    kc_u32 s = KcFilePipeline(&v, Req->PathA, Req->PathALength);
    r.Status = s;
    r.Length = (s == KC_OK) ? KcPack(r.NtPath, KC_MAX_PATH, v.Buf) : 0u;
    Rep->Status = s;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_Del(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW v = { 0 };
    kc_u32 bs = KcFilePipeline(&v, Req->PathA, Req->PathALength);
    if (bs != KC_OK) { Rep->Status = bs; Rep->Bytes = 0; return STATUS_SUCCESS; }
    (VOID)KcDel(v.Buf, &bs);
    Rep->Status = bs;
    Rep->Bytes = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS H_Copy(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW a = { 0 }, b = { 0 };
    kc_u64 bytes = 0;
    NTSTATUS st;
    kc_u32 s = KcFilePipeline(&a, Req->PathA, Req->PathALength);
    if (s != KC_OK) { Rep->Status = s; Rep->Bytes = 0; return STATUS_SUCCESS; }
    s = KcFilePipeline(&b, Req->PathB, Req->PathBLength);
    if (s != KC_OK) { Rep->Status = s; Rep->Bytes = 0; return STATUS_SUCCESS; }
    st = KcCopy(a.Buf, b.Buf, &bytes);
    Rep->Status = KcNtToKc(st);
    Rep->Bytes = sizeof(bytes);
    RtlCopyMemory(Rep->Payload, &bytes, sizeof(bytes));
    return STATUS_SUCCESS;
}

static NTSTATUS H_Rd(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW v = { 0 };
    KC_RD_RESULT r = { 0 };
    kc_u32 s = KcFilePipeline(&v, Req->PathA, Req->PathALength);
    if (s != KC_OK) { Rep->Status = s; Rep->Bytes = 0; return STATUS_SUCCESS; }
    (VOID)KcRd(v.Buf, &r);
    Rep->Status = r.Status;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_Kill(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_TERM_RESULT r = { 0 };
    HANDLE pid = (HANDLE)(ULONG_PTR)Req->Value;
    (VOID)KcTerminate(pid, Req->Flags, &r);
    Rep->Status = r.Status;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_Protect(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PROTECT_INFO info = { 0 };
    HANDLE pid = (HANDLE)(ULONG_PTR)Req->Value;
    (VOID)KcQueryProtection(pid, &info);
    Rep->Status = info.Error ? info.Error : KC_OK;
    Rep->Bytes = sizeof(info);
    RtlCopyMemory(Rep->Payload, &info, sizeof(info));
    return STATUS_SUCCESS;
}

static NTSTATUS H_StripProtection(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_STRIP_RESULT r = { 0 };
    HANDLE pid = (HANDLE)(ULONG_PTR)Req->Value;
    (VOID)KcStripProtection(pid, Req->Flags, &r);
    Rep->Status = r.Status;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_Critical(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_CRITICAL_RESULT r = { 0 };
    HANDLE pid = (HANDLE)(ULONG_PTR)Req->Value;
    ULONG op = Req->Flags & (KC_CRIT_OP_QUERY | KC_CRIT_OP_SET | KC_CRIT_OP_CLEAR);
    (VOID)KcCriticalOp(pid, op, Req->Flags, &r);
    Rep->Status = r.Status;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_CallbackOn(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    ULONG mask = (ULONG)Req->Value;
    NTSTATUS st;
    if (!mask) mask = KC_CB_PROC | KC_CB_REG | KC_CB_HANDLE | KC_CB_FILE;
    st = KcCallbacksEnable(mask);
    Rep->Status = KcNtToKc(st);
    Rep->Bytes = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS H_CallbackOff(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    ULONG mask = (ULONG)Req->Value;
    if (!mask) mask = KC_CB_PROC | KC_CB_REG | KC_CB_HANDLE | KC_CB_FILE;
    KcCallbacksDisable(mask);
    Rep->Status = KC_OK;
    Rep->Bytes = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS H_CallbackPoll(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_EVENT_BATCH* b;
    ULONG n;
    UNREFERENCED_PARAMETER(Req);
    b = (KC_EVENT_BATCH*)KcAlloc(sizeof(KC_EVENT_BATCH));
    if (!b) { Rep->Status = KC_ERR_ALLOC_FAILED; Rep->Bytes = 0; return STATUS_SUCCESS; }
    n = KcLegacyPull(b);
    Rep->Status = KC_OK;
    Rep->Bytes = (kc_u32)(offsetof(KC_EVENT_BATCH, Events) + (SIZE_T)n * sizeof(KC_EVENT));
    RtlCopyMemory(Rep->Payload, b, Rep->Bytes);
    KcFree(b);
    return STATUS_SUCCESS;
}

static NTSTATUS H_EventPull(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_EVENT_BATCH* b;
    ULONG n;
    UNREFERENCED_PARAMETER(Req);
    b = (KC_EVENT_BATCH*)KcAlloc(sizeof(KC_EVENT_BATCH));
    if (!b) { Rep->Status = KC_ERR_ALLOC_FAILED; Rep->Bytes = 0; return STATUS_SUCCESS; }
    n = KcFanoutPull(b);
    Rep->Status = KC_OK;
    Rep->Bytes = (kc_u32)(offsetof(KC_EVENT_BATCH, Events) + (SIZE_T)n * sizeof(KC_EVENT));
    RtlCopyMemory(Rep->Payload, b, Rep->Bytes);
    KcFree(b);
    return STATUS_SUCCESS;
}

static NTSTATUS H_RegPath(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW v = { 0 };
    KC_PATH_RESULT r = { 0 };
    kc_u32 s = KcRegPipeline(&v, Req->PathA, Req->PathALength, FALSE);
    r.Status = s;
    r.Length = (s == KC_OK) ? KcPack(r.NtPath, KC_MAX_PATH, v.Buf) : 0u;
    Rep->Status = s;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_RegQuery(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW p = { 0 }, val = { 0 };
    KC_REG_RESULT r = { 0 };
    kc_u32 s = KcRegPipeline(&p, Req->PathA, Req->PathALength,
        (Req->Flags & KC_FLAG_WOW32) ? TRUE : FALSE);
    if (s != KC_OK) { Rep->Status = s; Rep->Bytes = 0; return STATUS_SUCCESS; }
    s = KcUnpackTo(&val, Req->PathB, Req->PathBLength);
    if (s != KC_OK) { Rep->Status = s; Rep->Bytes = 0; return STATUS_SUCCESS; }
    if (val.Buf[0] == L'\0') { Rep->Status = KC_ERR_BAD_INPUT; Rep->Bytes = 0; return STATUS_SUCCESS; }
    (VOID)KcRegQuery(p.Buf, val.Buf, &r);
    Rep->Status = r.Status;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_FileStat(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_FILE_STAT s = { 0 };
    ULONG i;
    UNREFERENCED_PARAMETER(Req);
    s.FilterRegistered = g.Flt.Filter ? 1u : 0u;
    s.FilteringStarted = g.Flt.Started ? 1u : 0u;
    s.InstanceCount = (kc_u32)g.Flt.InstanceCount;
    s.Observing = (g.Cb.Mask & KC_CB_FILE) ? 1u : 0u;
    s.AttachStatus = g.Flt.LastStatus;
    s.RuleCount = (kc_u32)g.Fp.RuleCount;
    s.Blocked = (kc_u32)g.Fp.Blocked;
    for (i = 0; i < (ULONG)KC_FLT_OPS; ++i) s.Ops[i] = (kc_u32)g.Flt.Ops[i];
    Rep->Status = KC_OK;
    Rep->Bytes = sizeof(s);
    RtlCopyMemory(Rep->Payload, &s, sizeof(s));
    return STATUS_SUCCESS;
}

static NTSTATUS H_FProtect(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW v = { 0 };
    KC_FP_RESULT* r;
    ULONG op = Req->Flags;
    kc_u32 s;
    if (op != KC_FP_ADD && op != KC_FP_REMOVE && op != KC_FP_CLEAR && op != KC_FP_LIST) {
        Rep->Status = KC_ERR_BAD_INPUT;
        Rep->Bytes = 0;
        return STATUS_SUCCESS;
    }
    r = (KC_FP_RESULT*)KcAlloc(sizeof(KC_FP_RESULT));
    if (!r) { Rep->Status = KC_ERR_ALLOC_FAILED; Rep->Bytes = 0; return STATUS_SUCCESS; }
    if (op == KC_FP_ADD || op == KC_FP_REMOVE) {
        s = KcFilePipeline(&v, Req->PathA, Req->PathALength);
        if (s != KC_OK) {
            KcFree(r);
            Rep->Status = s;
            Rep->Bytes = 0;
            return STATUS_SUCCESS;
        }
    }
    s = KcFpApply(v.Buf, op, r);
    Rep->Status = s;
    Rep->Bytes = sizeof(*r);
    RtlCopyMemory(Rep->Payload, r, sizeof(*r));
    KcFree(r);
    return STATUS_SUCCESS;
}

static NTSTATUS H_EnumOpen(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_PATH_VIEW v = { 0 };
    KC_ENUM_OPEN_RESULT r = { 0 };
    ULONG kind = (ULONG)(Req->Value & 0xFF);
    ULONG pid = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
    kc_u32 s;
    if (kind == KC_ENUM_KIND_DIR) {
        s = KcFilePipeline(&v, Req->PathA, Req->PathALength);
        if (s != KC_OK) { Rep->Status = s; Rep->Bytes = 0; return STATUS_SUCCESS; }
    }
    s = KcEnumOpen(kind, v.Buf, pid, Req->Flags, &r);
    Rep->Status = r.Status ? r.Status : s;
    Rep->Bytes = sizeof(r);
    RtlCopyMemory(Rep->Payload, &r, sizeof(r));
    return STATUS_SUCCESS;
}

static NTSTATUS H_EnumNext(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    KC_ENUM_PAGE* page;
    ULONG session = (ULONG)Req->Value;
    ULONG cursor = Req->Flags;
    ULONG pid = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
    page = (KC_ENUM_PAGE*)KcAlloc(sizeof(KC_ENUM_PAGE));
    if (!page) { Rep->Status = KC_ERR_ALLOC_FAILED; Rep->Bytes = 0; return STATUS_SUCCESS; }
    (VOID)KcEnumPage(session, cursor, 0, pid, page);
    Rep->Status = page->Status;
    Rep->Bytes = (kc_u32)(offsetof(KC_ENUM_PAGE, Items) +
        (SIZE_T)page->Count * sizeof(KC_ENUM_ITEM));
    if (Rep->Bytes > KC_PAYLOAD) Rep->Bytes = KC_PAYLOAD;
    RtlCopyMemory(Rep->Payload, page, Rep->Bytes);
    KcFree(page);
    return STATUS_SUCCESS;
}

static NTSTATUS H_EnumClose(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    ULONG pid = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
    Rep->Status = KcEnumClose((ULONG)Req->Value, pid);
    Rep->Bytes = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS KcEnumLegacy(const KC_REQUEST* Req, KC_REPLY* Rep, ULONG kind)
{
    KC_PATH_VIEW v = { 0 };
    KC_ENUM_OPEN_RESULT o = { 0 };
    KC_ENUM_PAGE* page;
    ULONG session, cursor;
    ULONG pid = (ULONG)(ULONG_PTR)PsGetCurrentProcessId();
    kc_u32 s;
    page = (KC_ENUM_PAGE*)KcAlloc(sizeof(KC_ENUM_PAGE));
    if (!page) { Rep->Status = KC_ERR_ALLOC_FAILED; Rep->Bytes = 0; return STATUS_SUCCESS; }
    session = (ULONG)(Req->Value >> 32);
    cursor = (ULONG)Req->Value;
    if (cursor == 0) {
        if (kind == KC_ENUM_KIND_DIR) {
            s = KcFilePipeline(&v, Req->PathA, Req->PathALength);
            if (s != KC_OK) {
                KcFree(page); Rep->Status = s; Rep->Bytes = 0;
                return STATUS_SUCCESS;
            }
        }
        s = KcEnumOpen(kind, v.Buf, pid, 0, &o);
        if (s != KC_OK) {
            KcFree(page); Rep->Status = s; Rep->Bytes = 0;
            return STATUS_SUCCESS;
        }
        session = o.Session;
    }
    (VOID)KcEnumPage(session, cursor, kind, pid, page);
    Rep->Status = page->Status;
    Rep->Bytes = (kc_u32)(offsetof(KC_ENUM_PAGE, Items) +
        (SIZE_T)page->Count * sizeof(KC_ENUM_ITEM));
    if (Rep->Bytes > KC_PAYLOAD) Rep->Bytes = KC_PAYLOAD;
    RtlCopyMemory(Rep->Payload, page, Rep->Bytes);
    KcFree(page);
    return STATUS_SUCCESS;
}

static NTSTATUS H_EnumProc(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    return KcEnumLegacy(Req, Rep, KC_ENUM_KIND_PROC);
}

static NTSTATUS H_EnumDir(const KC_REQUEST* Req, KC_REPLY* Rep)
{
    return KcEnumLegacy(Req, Rep, KC_ENUM_KIND_DIR);
}

static const struct { ULONG Code; KC_HANDLER Fn; } g_Handlers[] = {
    { IOCTL_KC_PING,              H_Ping },
    { IOCTL_KC_RESOLVE_PATH,      H_ResolvePath },
    { IOCTL_KC_DEL,               H_Del },
    { IOCTL_KC_COPY,              H_Copy },
    { IOCTL_KC_RD,                H_Rd },
    { IOCTL_KC_KILL,              H_Kill },
    { IOCTL_KC_PROTECT,           H_Protect },
    { IOCTL_KC_CALLBACK_ON,       H_CallbackOn },
    { IOCTL_KC_CALLBACK_OFF,      H_CallbackOff },
    { IOCTL_KC_CALLBACK_POLL,     H_CallbackPoll },
    { IOCTL_KC_REGPATH,           H_RegPath },
    { IOCTL_KC_REGQUERY,          H_RegQuery },
    { IOCTL_KC_FILE_STAT,         H_FileStat },
    { IOCTL_KC_FPROTECT,          H_FProtect },
    { IOCTL_KC_ENUM_PROC,         H_EnumProc },
    { IOCTL_KC_ENUM_DIR,          H_EnumDir },
    { IOCTL_KC_EVENT_PULL,        H_EventPull },
    { IOCTL_KC_STRIP_PROTECTION,  H_StripProtection },
    { IOCTL_KC_CRITICAL,          H_Critical },
    { IOCTL_KC_ENUM_OPEN,         H_EnumOpen },
    { IOCTL_KC_ENUM_NEXT,         H_EnumNext },
    { IOCTL_KC_ENUM_CLOSE,        H_EnumClose },
    { IOCTL_KC_STATUS,            H_Status }
};

static NTSTATUS KcDispatchCreateClose(PDEVICE_OBJECT Dev, PIRP Irp)
{
    BOOLEAN entered;
    NTSTATUS st;
    UNREFERENCED_PARAMETER(Dev);
    entered = KcRequestEnter();
    st = entered ? STATUS_SUCCESS : STATUS_DELETE_PENDING;
    Irp->IoStatus.Status = st;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    if (entered) KcRequestExit();
    return st;
}

static NTSTATUS KcDispatchIoctl(PDEVICE_OBJECT Dev, PIRP Irp)
{
    PIO_STACK_LOCATION st = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = st->Parameters.DeviceIoControl.IoControlCode;
    ULONG inLen = st->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = st->Parameters.DeviceIoControl.OutputBufferLength;
    KC_REQUEST* req = NULL;
    KC_REPLY* rep = NULL;
    NTSTATUS trans = STATUS_SUCCESS;
    ULONG i, info = 0;
    UNREFERENCED_PARAMETER(Dev);

    if (!KcRequestEnter()) {
        Irp->IoStatus.Status = STATUS_DELETE_PENDING;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DELETE_PENDING;
    }

    if (outLen < sizeof(KC_REPLY) || inLen < sizeof(KC_REQUEST)) {
        trans = (outLen < sizeof(KC_REPLY)) ? STATUS_BUFFER_TOO_SMALL : STATUS_INVALID_PARAMETER;
        goto complete;
    }
    req = (KC_REQUEST*)KcAlloc(sizeof(KC_REQUEST));
    rep = (KC_REPLY*)KcAlloc(sizeof(KC_REPLY));
    if (!req || !rep) {
        if (req) KcFree(req);
        if (rep) KcFree(rep);
        req = NULL; rep = NULL;
        trans = STATUS_INSUFFICIENT_RESOURCES;
        goto complete;
    }
    RtlZeroMemory(req, sizeof(KC_REQUEST));
    RtlZeroMemory(rep, sizeof(KC_REPLY));
    RtlCopyMemory(req, Irp->AssociatedIrp.SystemBuffer, sizeof(KC_REQUEST));

    rep->Status = KC_ERR_UNSUPPORTED;
    rep->Bytes = 0;

    if (req->Command != 0 && req->Command != code) {
        rep->Status = KC_ERR_BAD_COMMAND;
    }
    else if (KcGateBlocks(code)) {
        rep->Status = KC_ERR_BUSY;
    }
    else {
        for (i = 0; i < sizeof(g_Handlers) / sizeof(g_Handlers[0]); ++i) {
            if (g_Handlers[i].Code == code) {
                trans = g_Handlers[i].Fn(req, rep);
                break;
            }
        }
    }
    RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, rep, sizeof(KC_REPLY));
    KcFree(rep);
    KcFree(req);
    rep = NULL; req = NULL;
    info = sizeof(KC_REPLY);

complete:
    KcRequestExit();
    Irp->IoStatus.Status = trans;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return trans;
}

static VOID KcUnload(PDRIVER_OBJECT Drv)
{
    UNREFERENCED_PARAMETER(Drv);
    KcRequestDrain();
    KcCallbacksDisableAll();
    KcFanoutStop();
    KcFltStop();
    IoDeleteSymbolicLink(&g_SymLink);
    if (g.DeviceObject) IoDeleteDevice(g.DeviceObject);
    g.DeviceObject = NULL;
    KcDeleteSync();
    g.Lifecycle.State = KC_LC_DEAD;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT Drv, PUNICODE_STRING RegPath)
{
    NTSTATUS st;
    UNREFERENCED_PARAMETER(RegPath);
    RtlZeroMemory(&g, sizeof(g));
    KcInitNames();
    KcInitSync();
    KcInitApis();

    {
        PFN_IO_CREATE_DEVICE_SECURE createDev =
            (PFN_IO_CREATE_DEVICE_SECURE)KcGetProc(L"IoCreateDeviceSecure");
        if (!createDev) {
            DbgPrint("[Kernel_Tools] IoCreateDeviceSecure not exported; "
                "refusing to create a device without SDDL gate\n");
            KcDeleteSync();
            return STATUS_NOT_SUPPORTED;
        }
        st = createDev(Drv, 0, &g_DevName,
            FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE,
            &g_Sddl, NULL, &g.DeviceObject);
    }
    if (!NT_SUCCESS(st)) { KcDeleteSync(); return st; }

    st = IoCreateSymbolicLink(&g_SymLink, &g_DevName);
    if (!NT_SUCCESS(st)) {
        IoDeleteDevice(g.DeviceObject);
        g.DeviceObject = NULL;
        KcDeleteSync();
        return st;
    }

    Drv->MajorFunction[IRP_MJ_CREATE] = KcDispatchCreateClose;
    Drv->MajorFunction[IRP_MJ_CLOSE] = KcDispatchCreateClose;
    Drv->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KcDispatchIoctl;
    Drv->DriverUnload = KcUnload;

    (VOID)KcFltInit(Drv);
    return STATUS_SUCCESS;
}
