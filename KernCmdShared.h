#ifndef KERNCMD_SHARED_H
#define KERNCMD_SHARED_H

typedef unsigned char      kc_u8;
typedef unsigned short     kc_u16;
typedef unsigned int       kc_u32;
typedef unsigned long long kc_u64;

enum {
    KC_MAX_PATH = 512,
    KC_DETAIL = 128,
    KC_RING = 56,
    KC_BATCH_MAX = 14,
    KC_PAYLOAD = 4096,
    KC_REG_DATA = 512,
    KC_FLT_OPS = 32,


    IOCTL_KC_PING = 0x00222000,
    IOCTL_KC_RESOLVE_PATH = 0x00222004,
    IOCTL_KC_DEL = 0x00222008,
    IOCTL_KC_COPY = 0x0022200C,
    IOCTL_KC_RD = 0x00222010,
    IOCTL_KC_KILL = 0x00222014,
    IOCTL_KC_PROTECT = 0x00222018,
    IOCTL_KC_CALLBACK_ON = 0x0022201C,
    IOCTL_KC_CALLBACK_OFF = 0x00222020,
    IOCTL_KC_CALLBACK_POLL = 0x00222024,
    IOCTL_KC_REGPATH = 0x00222028,
    IOCTL_KC_REGQUERY = 0x0022202C,
    IOCTL_KC_FILE_STAT = 0x00222030,
    IOCTL_KC_FPROTECT = 0x00222034,


    IOCTL_KC_ENUM_PROC = 0x00222038,
    IOCTL_KC_ENUM_DIR = 0x0022203C,
    IOCTL_KC_EVENT_PULL = 0x00222040,


    IOCTL_KC_STRIP_PROTECTION = 0x00222044,
    IOCTL_KC_CRITICAL = 0x00222048,
    IOCTL_KC_ENUM_OPEN = 0x0022204C,
    IOCTL_KC_ENUM_NEXT = 0x00222050,
    IOCTL_KC_ENUM_CLOSE = 0x00222054,
    IOCTL_KC_STATUS = 0x00222058,


    KC_KILL_REFUSE = 0x1,
    KC_KILL_NOPPL = 0x2,
    KC_FLAG_WOW32 = 0x4,


    KC_TERM_ALLOW_CRITICAL = 0x8,


    KC_STRIP_ALLOW_CRITICAL = 0x10,


    KC_CRIT_OP_QUERY = 0x1,
    KC_CRIT_OP_SET = 0x2,
    KC_CRIT_OP_CLEAR = 0x4,
    KC_CRIT_ALLOW_CLEAR = 0x8,


    KC_ENUM_STRICT_LIVENESS = 0x1,
    KC_ENUM_KIND_PROC = 1,
    KC_ENUM_KIND_DIR = 2,


    KC_FP_ADD = 0x1,
    KC_FP_REMOVE = 0x2,
    KC_FP_CLEAR = 0x4,
    KC_FP_LIST = 0x8,
    KC_FP_MAX = 3,


    KC_EVT_PROC = 1,
    KC_EVT_REG = 2,
    KC_EVT_HANDLE = 3,
    KC_EVT_FILE = 4,
    KC_CB_PROC = 0x1,
    KC_CB_REG = 0x2,
    KC_CB_HANDLE = 0x4,
    KC_CB_FILE = 0x8
};

#if defined(__cplusplus)

static constexpr kc_u32 KC_OK = 0x00000000u;
static constexpr kc_u32 KC_ERR_GENERIC = 0xE0000001u;
static constexpr kc_u32 KC_ERR_UNSUPPORTED = 0xE0000002u;
static constexpr kc_u32 KC_ERR_BAD_INPUT = 0xE0000003u;
static constexpr kc_u32 KC_ERR_NOT_FOUND = 0xE0000004u;
static constexpr kc_u32 KC_ERR_DENIED = 0xE0000005u;
static constexpr kc_u32 KC_ERR_OPEN_FAILED = 0xE0000006u;
static constexpr kc_u32 KC_ERR_ALLOC_FAILED = 0xE0000007u;
static constexpr kc_u32 KC_ERR_EMPTY_PATH = 0xE0000010u;
static constexpr kc_u32 KC_ERR_PATH_NOT_ABS = 0xE0000011u;
static constexpr kc_u32 KC_ERR_PATH_DRIVE = 0xE0000012u;
static constexpr kc_u32 KC_ERR_PATH_LONG = 0xE0000013u;
static constexpr kc_u32 KC_ERR_PATH_SYNTAX = 0xE0000014u;
static constexpr kc_u32 KC_ERR_REG_NOT_ABS = 0xE0000020u;
static constexpr kc_u32 KC_ERR_REG_CONTEXT = 0xE0000021u;
static constexpr kc_u32 KC_ERR_REG_DENIED = 0xE0000022u;
static constexpr kc_u32 KC_ERR_REG_SYNTAX = 0xE0000023u;
static constexpr kc_u32 KC_ERR_API_MISSING = 0xE0000030u;
static constexpr kc_u32 KC_ERR_OFFSET_FAIL = 0xE0000031u;
static constexpr kc_u32 KC_ERR_CRITICAL_REFUSE = 0xE0000040u;
static constexpr kc_u32 KC_ERR_CRITICAL_CLEAR = 0xE0000041u;
static constexpr kc_u32 KC_ERR_FP_FULL = 0xE0000050u;
static constexpr kc_u32 KC_ERR_FP_NOT_FOUND = 0xE0000051u;
static constexpr kc_u32 KC_ERR_BUSY = 0xE0000060u;
static constexpr kc_u32 KC_ERR_GONE = 0xE0000061u;
static constexpr kc_u32 KC_ERR_PPL_STRIP = 0xE0000062u;
static constexpr kc_u32 KC_ERR_PPL_GONE = 0xE0000063u;
static constexpr kc_u32 KC_ERR_PPL_PROTECTED = 0xE0000064u;
static constexpr kc_u32 KC_ERR_FP_BLOCKED = 0xE0000065u;
static constexpr kc_u32 KC_ERR_ENUM_INVALID = 0xE0000066u;
static constexpr kc_u32 KC_ERR_UNLOADING = 0xE0000067u;
static constexpr kc_u32 KC_ERR_NOT_OWNER = 0xE0000068u;
static constexpr kc_u32 KC_ERR_SESSION_CLOSED = 0xE0000069u;
static constexpr kc_u32 KC_ERR_PARTIAL = 0xE000006Au;
static constexpr kc_u32 KC_ERR_REFUSED = 0xE000006Bu;
static constexpr kc_u32 KC_ERR_NO_SESSION = 0xE000006Cu;
static constexpr kc_u32 KC_ERR_UNSAFE_TARGET = 0xE000006Du;
static constexpr kc_u32 KC_ERR_LIFECYCLE = 0xE000006Eu;
static constexpr kc_u32 KC_ERR_BAD_COMMAND = 0xE000006Fu;
static constexpr kc_u32 KC_ERR_ALREADY = 0xE0000070u;
static constexpr kc_u32 KC_ERR_PLAN_FULL = 0xE0000071u;
#else
static const kc_u32 KC_OK = 0x00000000u;
static const kc_u32 KC_ERR_GENERIC = 0xE0000001u;
static const kc_u32 KC_ERR_UNSUPPORTED = 0xE0000002u;
static const kc_u32 KC_ERR_BAD_INPUT = 0xE0000003u;
static const kc_u32 KC_ERR_NOT_FOUND = 0xE0000004u;
static const kc_u32 KC_ERR_DENIED = 0xE0000005u;
static const kc_u32 KC_ERR_OPEN_FAILED = 0xE0000006u;
static const kc_u32 KC_ERR_ALLOC_FAILED = 0xE0000007u;
static const kc_u32 KC_ERR_EMPTY_PATH = 0xE0000010u;
static const kc_u32 KC_ERR_PATH_NOT_ABS = 0xE0000011u;
static const kc_u32 KC_ERR_PATH_DRIVE = 0xE0000012u;
static const kc_u32 KC_ERR_PATH_LONG = 0xE0000013u;
static const kc_u32 KC_ERR_PATH_SYNTAX = 0xE0000014u;
static const kc_u32 KC_ERR_REG_NOT_ABS = 0xE0000020u;
static const kc_u32 KC_ERR_REG_CONTEXT = 0xE0000021u;
static const kc_u32 KC_ERR_REG_DENIED = 0xE0000022u;
static const kc_u32 KC_ERR_REG_SYNTAX = 0xE0000023u;
static const kc_u32 KC_ERR_API_MISSING = 0xE0000030u;
static const kc_u32 KC_ERR_OFFSET_FAIL = 0xE0000031u;
static const kc_u32 KC_ERR_CRITICAL_REFUSE = 0xE0000040u;
static const kc_u32 KC_ERR_CRITICAL_CLEAR = 0xE0000041u;
static const kc_u32 KC_ERR_FP_FULL = 0xE0000050u;
static const kc_u32 KC_ERR_FP_NOT_FOUND = 0xE0000051u;
static const kc_u32 KC_ERR_BUSY = 0xE0000060u;
static const kc_u32 KC_ERR_GONE = 0xE0000061u;
static const kc_u32 KC_ERR_PPL_STRIP = 0xE0000062u;
static const kc_u32 KC_ERR_PPL_GONE = 0xE0000063u;
static const kc_u32 KC_ERR_PPL_PROTECTED = 0xE0000064u;
static const kc_u32 KC_ERR_FP_BLOCKED = 0xE0000065u;
static const kc_u32 KC_ERR_ENUM_INVALID = 0xE0000066u;
static const kc_u32 KC_ERR_UNLOADING = 0xE0000067u;
static const kc_u32 KC_ERR_NOT_OWNER = 0xE0000068u;
static const kc_u32 KC_ERR_SESSION_CLOSED = 0xE0000069u;
static const kc_u32 KC_ERR_PARTIAL = 0xE000006Au;
static const kc_u32 KC_ERR_REFUSED = 0xE000006Bu;
static const kc_u32 KC_ERR_NO_SESSION = 0xE000006Cu;
static const kc_u32 KC_ERR_UNSAFE_TARGET = 0xE000006Du;
static const kc_u32 KC_ERR_LIFECYCLE = 0xE000006Eu;
static const kc_u32 KC_ERR_BAD_COMMAND = 0xE000006Fu;
static const kc_u32 KC_ERR_ALREADY = 0xE0000070u;
static const kc_u32 KC_ERR_PLAN_FULL = 0xE0000071u;
#endif

typedef struct _KC_REQUEST {
    kc_u32 Command;
    kc_u32 Flags;
    kc_u64 Value;
    kc_u32 PathALength;
    kc_u32 PathBLength;
    kc_u16 PathA[KC_MAX_PATH];
    kc_u16 PathB[KC_MAX_PATH];
} KC_REQUEST;

typedef struct _KC_REPLY {
    kc_u32 Status;
    kc_u32 Bytes;
    kc_u32 Reserved;
    kc_u32 Reserved2;
    kc_u8  Payload[KC_PAYLOAD];
} KC_REPLY;

typedef struct _KC_EVENT {
    kc_u64 TimeStamp;
    kc_u32 Type;
    kc_u32 ProcessId;
    kc_u32 Operation;
    kc_u32 Status;
    kc_u16 Detail[KC_DETAIL];
} KC_EVENT;

typedef struct _KC_EVENT_BATCH {
    kc_u32 Count;
    kc_u32 Reserved;
    KC_EVENT Events[KC_BATCH_MAX];
} KC_EVENT_BATCH;

typedef struct _KC_PATH_RESULT {
    kc_u32 Status;
    kc_u32 Length;
    kc_u16 NtPath[KC_MAX_PATH];
} KC_PATH_RESULT;

typedef struct _KC_KILL_INFO {
    kc_u32 Status;
    kc_u32 WasCritical;
    kc_u32 CriticalCleared;
    kc_u32 PplCleared;
    kc_u32 ProtectionOffset;
    kc_u32 ProtectionBefore;
    kc_u32 ProtectionAfter;
    kc_u32 Reserved;
} KC_KILL_INFO;

typedef struct _KC_PROTECT_INFO {
    kc_u32 Pid;
    kc_u8  Type;
    kc_u8  Audit;
    kc_u8  Signer;
    kc_u8  Raw;
    kc_u32 ProtectionOffset;
    kc_u32 Error;
} KC_PROTECT_INFO;

typedef struct _KC_REG_RESULT {
    kc_u32 Status;
    kc_u32 ValueType;
    kc_u32 DataLength;
    kc_u32 Reserved;
    kc_u8  Data[KC_REG_DATA];
} KC_REG_RESULT;

typedef struct _KC_FILE_STAT {
    kc_u32 FilterRegistered;
    kc_u32 FilteringStarted;
    kc_u32 InstanceCount;
    kc_u32 Observing;
    kc_u32 Ops[KC_FLT_OPS];
    kc_u32 AttachStatus;
    kc_u32 RuleCount;
    kc_u32 Blocked;
} KC_FILE_STAT;

typedef struct _KC_FP_RESULT {
    kc_u32 Status;
    kc_u32 Count;
    kc_u32 Blocked;
    kc_u32 Observed;
    kc_u16 Rules[KC_FP_MAX][KC_MAX_PATH];
} KC_FP_RESULT;

#define KC_ENUM_SLOTS       4
#define KC_ENUM_ITEM_MAX    12
#define KC_ENUM_NAME        128

typedef struct _KC_ENUM_ITEM {
    kc_u32 Id;
    kc_u32 ParentId;
    kc_u32 Flags;
    kc_u32 Reserved;
    kc_u64 Size;
    kc_u16 Name[KC_ENUM_NAME];
} KC_ENUM_ITEM;

typedef struct _KC_ENUM_PAGE {
    kc_u32 Status;
    kc_u32 Session;
    kc_u32 Total;
    kc_u32 BatchNo;
    kc_u32 Count;
    kc_u32 More;
    kc_u32 Reserved;
    kc_u32 Reserved2;
    KC_ENUM_ITEM Items[KC_ENUM_ITEM_MAX];
} KC_ENUM_PAGE;

typedef struct _KC_STATUS_RESULT {
    kc_u32 Status;
    kc_u32 Capabilities;
    kc_u32 Lifecycle;
    kc_u32 ActiveRequests;
    kc_u32 BuildNumber;
    kc_u32 LegacyDropped;
    kc_u32 FanoutSlots;
    kc_u32 EnumOpen;
    kc_u32 CallbackMask;
    kc_u32 FltState;
    kc_u32 Reserved;
} KC_STATUS_RESULT;

typedef struct _KC_TERM_RESULT {
    kc_u32 Status;
    kc_u32 Pid;
    kc_u32 WasCritical;
    kc_u32 Refused;
    kc_u32 Reserved;
} KC_TERM_RESULT;

typedef struct _KC_STRIP_RESULT {
    kc_u32 Status;
    kc_u32 Pid;
    kc_u32 ProtectionOffset;
    kc_u32 ProtectionBefore;
    kc_u32 ProtectionAfter;
    kc_u32 WasCritical;
    kc_u32 Restored;
    kc_u32 AlreadyClear;
} KC_STRIP_RESULT;

typedef struct _KC_CRITICAL_RESULT {
    kc_u32 Status;
    kc_u32 Pid;
    kc_u32 Operation;
    kc_u32 Before;
    kc_u32 After;
    kc_u32 Restored;
    kc_u32 Reserved;
} KC_CRITICAL_RESULT;

typedef struct _KC_RD_RESULT {
    kc_u32 Status;
    kc_u32 DeletedFiles;
    kc_u32 DeletedDirs;
    kc_u32 Failed;
    kc_u32 Partial;
} KC_RD_RESULT;

typedef struct _KC_ENUM_OPEN_RESULT {
    kc_u32 Status;
    kc_u32 Session;
    kc_u32 Total;
    kc_u32 Kind;
    kc_u32 Reserved;
} KC_ENUM_OPEN_RESULT;

#if defined(__cplusplus)
#define KC_ASSERT(c, n) static_assert((c), #n)
#else
#define KC_ASSERT(c, n) typedef char kc_static_assert_##n[(c) ? 1 : -1]
#endif

KC_ASSERT(sizeof(KC_ENUM_ITEM) == 280, enum_item_size);
KC_ASSERT(sizeof(KC_ENUM_PAGE) <= KC_PAYLOAD, enum_page_fits);
KC_ASSERT(sizeof(KC_EVENT_BATCH) <= KC_PAYLOAD, event_batch_fits);
KC_ASSERT(sizeof(KC_FP_RESULT) <= KC_PAYLOAD, fp_result_fits);
KC_ASSERT(sizeof(KC_REQUEST) == 2072, request_size);
KC_ASSERT(sizeof(KC_REPLY) == 4112, reply_size);
KC_ASSERT(sizeof(KC_STATUS_RESULT) <= KC_PAYLOAD, status_result_fits);
KC_ASSERT(sizeof(KC_TERM_RESULT) <= KC_PAYLOAD, term_result_fits);
KC_ASSERT(sizeof(KC_STRIP_RESULT) <= KC_PAYLOAD, strip_result_fits);
KC_ASSERT(sizeof(KC_CRITICAL_RESULT) <= KC_PAYLOAD, critical_result_fits);
KC_ASSERT(sizeof(KC_RD_RESULT) <= KC_PAYLOAD, rd_result_fits);
KC_ASSERT(sizeof(KC_ENUM_OPEN_RESULT) <= KC_PAYLOAD, enum_open_result_fits);

#endif
