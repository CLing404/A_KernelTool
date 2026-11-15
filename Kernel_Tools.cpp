#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <winver.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <userenv.h>
#include <shellapi.h>

#include "ExampleFluentGUI.h"
#include "KernCmdShared.h"

#pragma comment(linker, "/subsystem:windows /entry:wWinMainCRTStartup")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shell32.lib")

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

    const wchar_t* kDefaultDevice = L"\\\\.\\Kernel_Tools";

    const float kTitleH = 42.f;
    const float kToolbarH = 42.f;
    const float kNavW = 68.f;
    const float kBottomH = 220.f;

    enum class Pending {
        None, Del, Rmtree, Kill, Strip, CriticalToggle
    };

    struct IconCache {
        std::map<std::wstring, HICON> map{};
        std::set<std::wstring> failed{};
        IconCache() = default;
        IconCache(const IconCache&) = delete;
        IconCache& operator=(const IconCache&) = delete;
        ~IconCache() { Clear(); }
        void Clear() noexcept {
            for (auto& kv : map) if (kv.second) DestroyIcon(kv.second);
            map.clear();
            failed.clear();
        }
    };

    struct VerInfo {
        std::wstring company = L"-";
        std::wstring desc = L"-";
    };

    struct ProcNode {
        kc_u32 pid = 0;
        kc_u32 parent = 0;
        std::uint64_t createTime = 0;
        kc_u32 raw = 0;
        std::wstring name;
        std::wstring path;
        std::wstring company = L"-";
        std::wstring desc = L"-";
        double cpu = 0.0;
        std::uint64_t mem = 0;
        std::uint64_t peakWs = 0;
        std::uint64_t virtualSize = 0;
        std::uint64_t privateBytes = 0;
        std::uint64_t pagefile = 0;
        std::uint64_t quotaPaged = 0;
        std::uint64_t quotaNonPaged = 0;
        std::uint32_t handles = 0;
        std::uint32_t threads = 0;
        std::uint32_t session = 0;
        std::uint64_t lastCpuTicks = 0;
        int state = 0;
        int age = 0;
        int deadAge = 0;
        bool hasChild = false;
    };

    struct AppState {
        int  page = 0;
        bool autoRefresh = true;
        float refreshMs = 400.f;
        bool zTopmost = true;
        bool zApplied = false;
        bool zGuard = false;
        bool zGuardApplied = false;
        float zGuardMs = 750.f;
        float zGuardMsApplied = 0.f;
        bool uiAccess = false;
        bool tiGroup = false;
        bool maskOverlay = false;
        bool masksEnabled = false;
        bool maskOverlayApplied = false;
        bool masksEnabledApplied = false;
        unsigned long long lastVisSig = 0;
        int maskTotal = 0, maskSkipped = 0, maskRedrawn = 0;
        int confirmZ = 0;
        int confirmZApplied = -1;
        std::wstring interactiveSid;
        std::wstring identityText = L"未检测";
        std::wstring bootText = L"标准启动";
        std::chrono::steady_clock::time_point lastIdentity{};
        std::wstring refreshMsText = L"400";
        bool autoPull = false;
        bool allowCritical = false;
        bool strictEnum = false;
        bool wow32 = false;
        int  theme = 0;
        int  eventQueue = 1;
        int  enumKind = 0;
        int  bottomTab = 0;
        int  bottomOffset = 0;
        int  sortCol = 0;
        bool sortAsc = true;
        int  ctxRow = -1;
        bool animEnabled = true;
        float fadeSec = 0.18f;
        float accentR = 0x60 / 255.f;
        float accentG = 0xCD / 255.f;
        float accentB = 0xFF / 255.f;
        float bgR = 0x20 / 255.f;
        float bgG = 0x21 / 255.f;
        float bgB = 0x24 / 255.f;

        std::array<std::wstring, 7> pageNames{
            L"进程", L"文件", L"注册表", L"事件", L"枚举", L"状态", L"设置"
        };
        std::array<const wchar_t*, 7> pageIcons{
            L"\uF133", L"\uF418", L"\uF4B6", L"\uF114",
            L"\uF8CB", L"\uE9DC", L"\uF6A9"
        };
        std::array<const wchar_t*, 7> pageIconsOn{
            L"\uF133", L"\uF41C", L"\uF4C0", L"\uF114",
            L"\uF8E3", L"\uE9E4", L"\uF6B2"
        };
        std::array<const wchar_t*, 7> pageIconsLegacy{
            L"\u25A4", L"\u25A6", L"\u25A3", L"\u25CE",
            L"\u2261", L"\u25C9", L"\u2699"
        };
        std::array<int, 7>   pageScroll{};
        std::array<float, 7> pageMeasured{};

        std::wstring devicePath = kDefaultDevice;
        std::wstring pidInput;
        std::wstring fileA;
        std::wstring fileB;
        std::wstring rmtreePath;
        std::wstring fpPath;
        std::wstring regPath;
        std::wstring regValue;
        std::wstring enumDir;
        std::wstring sessionInput;
        std::wstring filter;
        std::wstring lastAction = L"未执行操作";
        std::wstring resultText = L"无查询结果";
        std::wstring pendingText;
        std::wstring statusText = L"未连接";
        std::wstring hudText = L"CPU 使用率: -%   内存占用率: -%";

        Pending pending = Pending::None;

        std::map<kc_u32, ProcNode> nodes;
        IconCache icons;
        std::vector<std::pair<kc_u32, int>> order;
        std::set<kc_u32> collapsed;
        std::map<kc_u32, VerInfo> verCache;
        kc_u32 selectedPid = 0;

        std::uint64_t lastBusy = 0;
        std::uint64_t lastTotal = 0;
        std::uint64_t sysDelta = 0;
        double cpuPct = 0.0;
        std::uint32_t memPct = 0;
        bool primed = false;
        std::chrono::steady_clock::time_point lastRefresh{};
        bool lastConnected = false;

        std::vector<std::wstring> eventHeaders;
        std::vector<std::vector<std::wstring>> eventRows;
        std::vector<std::wstring> logs;

        std::vector<std::wstring> regHeaders;
        std::vector<std::vector<std::wstring>> regRows;
        std::vector<std::wstring> fpHeaders;
        std::vector<std::vector<std::wstring>> fpRows;
        std::vector<std::wstring> enumHeaders;
        std::vector<std::vector<std::wstring>> enumRows;
        std::vector<std::wstring> statusHeaders;
        std::vector<std::vector<std::wstring>> statusRows;
        std::vector<std::wstring> fileStatHeaders;
        std::vector<std::vector<std::wstring>> fileStatRows;
        std::vector<std::wstring> bottomHeaders;
        std::vector<std::vector<std::wstring>> bottomRows;

        KC_STATUS_RESULT status{};
        kc_u32 enumSession = 0;
        kc_u32 enumCursor = 0;
        kc_u32 enumTotal = 0;
        kc_u32 enumMore = 0;

        bool cbProc = false;
        bool cbReg = false;
        bool cbHandle = false;
        bool cbFile = false;

        AppState() {
            eventHeaders = { L"类型", L"PID", L"操作", L"状态", L"时间", L"详情" };
            regHeaders = { L"字段", L"值" };
            fpHeaders = { L"序号", L"规则路径" };
            enumHeaders = { L"ID", L"标志", L"大小", L"名称" };
            statusHeaders = { L"字段", L"值" };
            fileStatHeaders = { L"项目", L"值" };
            bottomHeaders = { L"模块名", L"映像基址", L"映像大小", L"模块路径", L"公司名", L"文件描述" };
            logs.reserve(400);
        }
    };

    struct ThemePreset {
        example::U32 bgTop, bgBottom, panel, panelStroke;
        example::U32 control, controlHover, controlPress, stroke;
        example::U32 text, textMuted, textDim;
        example::U32 accent, accentPress, onAccent;
        example::U32 rowAlt, header, navBg, navActive;
    };

    const ThemePreset kThemes[4] = {
        { 0x191A1E, 0x202124, 0x2B2C30, 0x3A3B40,
          0x323338, 0x3D3E44, 0x484950, 0x4A4B52,
          0xF3F3F5, 0x9C9DA5, 0x6E6F77,
          0x60CDFF, 0x3FA8D8, 0x0C1116,
          0x2E2F34, 0x26272B, 0x242529, 0x35363C },
        { 0x0A0A0C, 0x101012, 0x161619, 0x2A2A30,
          0x1E1E22, 0x28282E, 0x34343C, 0x3A3A42,
          0xFAFAFA, 0xA0A0AA, 0x6A6A74,
          0xC77DFF, 0x9A55D8, 0x0C0810,
          0x18181C, 0x131316, 0x0E0E11, 0x1C1C22 },
        { 0x101A28, 0x16243A, 0x1C2C42, 0x2C4260,
          0x22344E, 0x2C4466, 0x385480, 0x3A5878,
          0xEAF2FF, 0x9CB4D0, 0x6C88A8,
          0x4CC2FF, 0x2E93D0, 0x061626,
          0x1E3048, 0x182840, 0x142236, 0x24405E },
        { 0x000000, 0x000000, 0x0C0C0C, 0x666666,
          0x1A1A1A, 0x303030, 0x444444, 0x808080,
          0xFFFFFF, 0xD0D0D0, 0x9A9A9A,
          0xFFE100, 0xC4AE00, 0x000000,
          0x141414, 0x101010, 0x080808, 0x242424 }
    };

    const wchar_t* kThemeNames[4] = { L"默认", L"纯黑", L"蓝调", L"高对比" };

    struct Swatch {
        const wchar_t* label;
        example::U32 rgb;
    };

    const Swatch kAccents[6] = {
        { L"蓝", 0x60CDFF },{ L"绿", 0x39FF14 },
        { L"橙", 0xFF9F43 },{ L"紫", 0xC77DFF },
        { L"粉", 0xFFB6C1 },{ L"黄", 0xFFE100 }
    };

    const Swatch kBackgrounds[6] = {
        { L"深灰", 0x202124 },{ L"纯黑", 0x000000 },
        { L"深蓝", 0x16243A },{ L"白", 0xF5F5F7 },
        { L"浅灰", 0xE8E8EC },{ L"米色", 0xF5F0E6 }
    };

    static bool Starts(std::wstring_view s, std::wstring_view p) {
        return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
    }

    static std::wstring Lower(std::wstring s) {
        for (auto& c : s) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        return s;
    }

    static std::wstring Hex(kc_u32 v) {
        wchar_t b[32];
        std::swprintf(b, 32, L"0x%08X", v);
        return b;
    }

    static std::wstring Hex64(std::uint64_t v) {
        wchar_t b[48];
        std::swprintf(b, 48, L"0x%016llX", (unsigned long long)v);
        return b;
    }

    static std::wstring Dec(std::uint64_t v) {
        return std::to_wstring((unsigned long long)v);
    }

    static std::wstring MemSize(std::uint64_t b) {
        wchar_t t[48];
        if (b < 1024ull * 1024ull)
            std::swprintf(t, 48, L"%.1fKB", (double)b / 1024.0);
        else if (b < 1024ull * 1024ull * 1024ull)
            std::swprintf(t, 48, L"%.1fMB", (double)b / 1048576.0);
        else
            std::swprintf(t, 48, L"%.2fGB", (double)b / 1073741824.0);
        return t;
    }

    static std::wstring CpuText(double v) {
        wchar_t t[32];
        std::swprintf(t, 32, L"%.2f", v);
        return t;
    }

    static std::wstring Unpack(const kc_u16* src, size_t n) {
        std::wstring r;
        r.reserve(n);
        for (size_t i = 0; i < n && src[i]; ++i) r.push_back((wchar_t)src[i]);
        return r;
    }

    static void PackPath(std::wstring_view s, kc_u16* dst, size_t cap, kc_u32& len) {
        size_t n = (std::min)(s.size(), cap - 1);
        for (size_t i = 0; i < n; ++i) dst[i] = (kc_u16)s[i];
        dst[n] = 0;
        len = (kc_u32)n;
    }

    static const wchar_t* StatusName(kc_u32 s) {
        struct E { kc_u32 c; const wchar_t* n; };
        static const E t[] = {
            { KC_ERR_GENERIC,         L"一般性失败" },
            { KC_ERR_UNSUPPORTED,     L"不支持的请求" },
            { KC_ERR_BAD_INPUT,       L"输入参数无效" },
            { KC_ERR_NOT_FOUND,       L"未找到" },
            { KC_ERR_DENIED,          L"拒绝访问" },
            { KC_ERR_OPEN_FAILED,     L"打开内核对象失败" },
            { KC_ERR_ALLOC_FAILED,    L"内存分配失败" },
            { KC_ERR_EMPTY_PATH,      L"路径为空" },
            { KC_ERR_PATH_NOT_ABS,    L"路径不是绝对 NT 路径" },
            { KC_ERR_PATH_DRIVE,      L"盘符路径依赖调用者 DOS 目录" },
            { KC_ERR_PATH_LONG,       L"路径超长" },
            { KC_ERR_PATH_SYNTAX,     L"路径语法非法" },
            { KC_ERR_REG_NOT_ABS,     L"注册表地址不是绝对路径" },
            { KC_ERR_REG_CONTEXT,     L"注册表地址上下文相关" },
            { KC_ERR_REG_DENIED,      L"敏感注册表键" },
            { KC_ERR_REG_SYNTAX,      L"注册表路径语法非法" },
            { KC_ERR_API_MISSING,     L"内核导出函数缺失" },
            { KC_ERR_OFFSET_FAIL,     L"EPROCESS.Protection 偏移定位失败" },
            { KC_ERR_CRITICAL_REFUSE, L"关键进程已拒绝，未做任何修改" },
            { KC_ERR_CRITICAL_CLEAR,  L"无法清除 BreakOnTermination" },
            { KC_ERR_FP_FULL,         L"保护规则已满" },
            { KC_ERR_FP_NOT_FOUND,    L"保护规则不存在" },
            { KC_ERR_BUSY,            L"另一实例正在枚举" },
            { KC_ERR_GONE,            L"枚举期间目标消失" },
            { KC_ERR_PPL_STRIP,       L"剥离失败，已完整回滚且未终止" },
            { KC_ERR_PPL_GONE,        L"进程在操作前已退出" },
            { KC_ERR_PPL_PROTECTED,   L"目标是受保护进程" },
            { KC_ERR_FP_BLOCKED,      L"被自身保护规则拦截" },
            { KC_ERR_ENUM_INVALID,    L"分页会话失效或游标越界" },
            { KC_ERR_UNLOADING,       L"驱动正在卸载" },
            { KC_ERR_NOT_OWNER,       L"会话不属于当前调用者" },
            { KC_ERR_SESSION_CLOSED,  L"会话已关闭" },
            { KC_ERR_PARTIAL,         L"部分完成" },
            { KC_ERR_REFUSED,         L"已拒绝" },
            { KC_ERR_NO_SESSION,      L"没有会话" },
            { KC_ERR_UNSAFE_TARGET,   L"目标不安全" },
            { KC_ERR_LIFECYCLE,       L"生命周期状态不允许" },
            { KC_ERR_BAD_COMMAND,     L"命令码不匹配" },
            { KC_ERR_ALREADY,         L"已处于目标状态" },
            { KC_ERR_PLAN_FULL,       L"计划已满" },
            { 0xC0000001u,            L"操作未成功" },
            { 0xC0000022u,            L"拒绝访问（可能被过滤驱动拦截）" },
            { 0xC0000034u,            L"对象不存在" },
            { 0xC000003Au,            L"路径不存在" },
            { 0xC0000043u,            L"文件被占用" },
            { 0xC00000BBu,            L"不支持" }
        };
        for (const auto& e : t) if (e.c == s) return e.n;
        return L"未知状态码";
    }

    static const wchar_t* SignerName(unsigned v) {
        static const wchar_t* n[] = {
            L"None", L"Authenticode", L"CodeGen", L"Antimalware",
            L"Lsa", L"Windows", L"WinTcb", L"?", L"?", L"?",
            L"?", L"?", L"?", L"?", L"?", L"?"
        };
        return n[v & 0xF];
    }

    static const wchar_t* EvtName(kc_u32 t) {
        switch (t) {
        case KC_EVT_PROC:   return L"proc";
        case KC_EVT_REG:    return L"reg";
        case KC_EVT_HANDLE: return L"handle";
        case KC_EVT_FILE:   return L"file";
        default:            return L"?";
        }
    }

    struct Conv {
        bool ok = false;
        std::wstring nt;
        std::wstring err;
    };

    static Conv OkC(std::wstring s) { return { true, std::move(s), L"" }; }
    static Conv BadC(const wchar_t* e) { return { false, L"", e }; }

    static Conv Win32ToNtPath(std::wstring p) {
        if (p.empty()) return BadC(L"路径为空");
        if (Starts(p, L"\\Device\\") || Starts(p, L"\\??\\") || Starts(p, L"\\DosDevices\\"))
            return OkC(p);
        if (Starts(p, L"\\\\?\\Volume{")) {
            size_t slash = p.find(L'\\', 10);
            if (slash == std::wstring::npos) return BadC(L"卷 GUID 路径缺少分隔符");
            std::wstring vol = p.substr(9, slash - 9);
            WCHAR dev[1024] = {};
            if (!QueryDosDeviceW(vol.c_str(), dev, 1024) || !dev[0])
                return BadC(L"卷无法映射到 NT 设备");
            return OkC(std::wstring(dev) + p.substr(slash));
        }
        if (Starts(p, L"\\\\?\\UNC\\")) return OkC(L"\\??\\UNC\\" + p.substr(8));
        if (Starts(p, L"\\\\.\\"))     return OkC(L"\\??\\" + p.substr(4));
        if (Starts(p, L"\\\\"))       return OkC(L"\\??\\UNC\\" + p.substr(2));
        if (p.size() >= 2 &&
            ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')) &&
            p[1] == L':') {
            if (p.size() == 2 || p[2] != L'\\') {
                WCHAR full[MAX_PATH * 2] = {};
                DWORD n = GetFullPathNameW(p.c_str(), MAX_PATH * 2, full, nullptr);
                if (!n || n >= MAX_PATH * 2) return BadC(L"路径展开失败");
                p = full;
            }
            std::wstring drive = p.substr(0, 2);
            WCHAR dev[1024] = {};
            DWORD n = QueryDosDeviceW(drive.c_str(), dev, 1024);
            if (!n || !dev[0]) return BadC(L"盘符无法映射");
            return OkC(std::wstring(dev) + p.substr(2));
        }
        WCHAR full[MAX_PATH * 2] = {};
        DWORD n = GetFullPathNameW(p.c_str(), MAX_PATH * 2, full, nullptr);
        if (!n || n >= MAX_PATH * 2) return BadC(L"路径展开失败");
        std::wstring f(full);
        bool drive = f.size() >= 3 && f[1] == L':' && f[2] == L'\\' &&
            ((f[0] >= L'A' && f[0] <= L'Z') || (f[0] >= L'a' && f[0] <= L'z'));
        if (!drive) return BadC(L"无法解析为绝对路径");
        return Win32ToNtPath(f);
    }

    static std::wstring NtToWin32Path(std::wstring_view nt) {
        if (nt.empty()) return L"";
        for (wchar_t d = L'A'; d <= L'Z'; ++d) {
            std::wstring drive = std::wstring(1, d) + L":";
            WCHAR dev[1024] = {};
            if (QueryDosDeviceW(drive.c_str(), dev, 1024) && dev[0]) {
                std::wstring target(dev);
                if (nt.size() >= target.size() &&
                    std::equal(target.begin(), target.end(), nt.begin()))
                    return drive + std::wstring(nt.substr(target.size()));
            }
        }
        if (Starts(nt, L"\\SystemRoot\\"))
            return L"C:\\Windows\\" + std::wstring(nt.substr(12));
        return std::wstring(nt);
    }

    static HICON LoadProcIcon(const std::wstring& path, const std::wstring& name) {
        constexpr UINT kIcon = 0x000000100u;
        constexpr UINT kUseFileAttributes = 0x000000010u;
        auto get = [](const std::wstring& p, UINT flags, DWORD attr) -> HICON {
            SHFILEINFOW f{};
            if (p.empty()) return nullptr;
            return SHGetFileInfoW(p.c_str(), attr, &f, sizeof(f), flags)
                ? f.hIcon : nullptr;
            };
        HICON h = nullptr;
        if (!path.empty() && path[0] != L'\\') {
            h = get(path, kIcon, 0);
            if (!h) {
                HICON hLarge = nullptr;
                if (ExtractIconExW(path.c_str(), 0, &hLarge, nullptr, 1) &&
                    hLarge) h = hLarge;
            }
        }
        if (!h && !name.empty()) {
            wchar_t root[512]{};
            const DWORD n = GetEnvironmentVariableW(L"SystemRoot", root, 512);
            if (n && n < 512) {
                std::wstring p = std::wstring(root, n) + L"\\System32\\" + name;
                h = get(p, kIcon, 0);
            }
        }
        if (!h) {
            const std::wstring& p = path.empty() ? name : path;
            h = get(p, kIcon | kUseFileAttributes,
                FILE_ATTRIBUTE_NORMAL);
        }
        return h;
    }

    static VerInfo ReadVersionInfo(const std::wstring& path) {
        VerInfo v;
        if (path.empty() || path[0] == L'\\') return v;
        DWORD sz = GetFileVersionInfoSizeW(path.c_str(), nullptr);
        if (!sz) return v;
        std::vector<std::uint8_t> buf(sz);
        if (!GetFileVersionInfoW(path.c_str(), 0, sz, buf.data())) return v;
        struct LangPage { unsigned short lang, page; } *lp = nullptr;
        UINT n = 0;
        if (!VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation",
            reinterpret_cast<void**>(&lp), &n) || n < sizeof(LangPage)) return v;
        wchar_t key[160];
        wchar_t* val = nullptr;
        UINT len = 0;
        std::swprintf(key, 160, L"\\StringFileInfo\\%04x%04x\\CompanyName",
            (unsigned)lp->lang, (unsigned)lp->page);
        if (VerQueryValueW(buf.data(), key, reinterpret_cast<void**>(&val), &len)
            && val && *val) v.company = val;
        std::swprintf(key, 160, L"\\StringFileInfo\\%04x%04x\\FileDescription",
            (unsigned)lp->lang, (unsigned)lp->page);
        if (VerQueryValueW(buf.data(), key, reinterpret_cast<void**>(&val), &len)
            && val && *val) v.desc = val;
        return v;
    }

    static unsigned long long Sig(std::initializer_list<unsigned long long> v) {
        unsigned long long h = 1469598103934665603ull;
        for (auto x : v) { h ^= x; h *= 1099511628211ull; }
        return h;
    }
    static unsigned long long HashW(const std::wstring& t) {
        unsigned long long h = 1469598103934665603ull;
        for (wchar_t c : t) { h ^= (unsigned long long)(unsigned)c; h *= 1099511628211ull; }
        return h;
    }
    static bool ExportMaskJson(const std::wstring& text, std::wstring& path) {
        WCHAR tmp[MAX_PATH]{};
        DWORD n = GetTempPathW(MAX_PATH, tmp);
        if (!n || n >= MAX_PATH) return false;
        path = std::wstring(tmp) + L"KernCmdMask.json";
        int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1,
            nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return false;
        std::string u8((size_t)bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1,
            u8.data(), bytes, nullptr, nullptr);
        HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        const char bom3[3] = { (char)0xEF, (char)0xBB, (char)0xBF };
        DWORD wr = 0;
        WriteFile(f, bom3, 3, &wr, nullptr);
        std::string body = u8.c_str();
        WriteFile(f, body.data(), (DWORD)body.size(), &wr, nullptr);
        CloseHandle(f);
        return true;
    }

    static std::optional<std::wstring> CurrentUserSid() {
        HANDLE tok = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return std::nullopt;
        DWORD need = 0;
        GetTokenInformation(tok, TokenUser, nullptr, 0, &need);
        std::vector<BYTE> buf(need);
        BOOL ok = GetTokenInformation(tok, TokenUser, buf.data(), need, &need);
        CloseHandle(tok);
        if (!ok) return std::nullopt;
        auto* tu = reinterpret_cast<TOKEN_USER*>(buf.data());
        LPWSTR sid = nullptr;
        if (!ConvertSidToStringSidW(tu->User.Sid, &sid)) return std::nullopt;
        std::wstring r(sid);
        LocalFree(sid);
        return r;
    }

    static bool HasUIAccess() {
        HANDLE tok = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
        DWORD v = 0, need = 0;
        BOOL ok = GetTokenInformation(tok, TokenUIAccess, &v, sizeof(v), &need);
        CloseHandle(tok);
        return ok && v != 0;
    }

    static const wchar_t* kTiSidText =
        L"S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";

    static bool TokenHasTrustedInstallerGroup(HANDLE tok) {
        DWORD need = 0;
        GetTokenInformation(tok, TokenGroups, nullptr, 0, &need);
        if (!need) return false;
        std::vector<BYTE> buf(need);
        if (!GetTokenInformation(tok, TokenGroups, buf.data(), need, &need))
            return false;
        PSID tiSid = nullptr;
        if (!ConvertStringSidToSidW(kTiSidText, &tiSid)) return false;
        auto* g = reinterpret_cast<TOKEN_GROUPS*>(buf.data());
        bool found = false;
        for (DWORD i = 0; i < g->GroupCount; ++i) {
            if (EqualSid(g->Groups[i].Sid, tiSid)) { found = true; break; }
        }
        LocalFree(tiSid);
        return found;
    }

    static std::wstring IdentityLine() {
        HANDLE tok = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_QUERY_SOURCE, &tok))
            return L"令牌查询失败";
        DWORD need = 0;
        std::wstring userSid = L"?";
        GetTokenInformation(tok, TokenUser, nullptr, 0, &need);
        std::vector<BYTE> ub(need);
        if (GetTokenInformation(tok, TokenUser, ub.data(), need, &need)) {
            auto* tu = reinterpret_cast<TOKEN_USER*>(ub.data());
            LPWSTR txt = nullptr;
            if (ConvertSidToStringSidW(tu->User.Sid, &txt)) {
                userSid = txt;
                LocalFree(txt);
            }
        }
        DWORD session = 0, uia = 0, il = 0;
        GetTokenInformation(tok, TokenSessionId, &session, sizeof(session), &need);
        GetTokenInformation(tok, TokenUIAccess, &uia, sizeof(uia), &need);
        GetTokenInformation(tok, TokenIntegrityLevel, nullptr, 0, &need);
        std::vector<BYTE> ib(need);
        if (GetTokenInformation(tok, TokenIntegrityLevel, ib.data(), need, &need)) {
            auto* ml = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(ib.data());
            if (ml && ml->Label.Sid) {
                DWORD sub = *GetSidSubAuthorityCount(ml->Label.Sid);
                il = *GetSidSubAuthority(ml->Label.Sid, sub - 1);
            }
        }
        const bool ti = TokenHasTrustedInstallerGroup(tok);
        CloseHandle(tok);
        wchar_t b[512];
        std::swprintf(b, 512,
            L"令牌用户 SID=%s  TrustedInstaller 组 SID=%s  会话 ID=%u  UIAccess=%s  完整性级别(IL)=0x%04X",
            userSid.c_str(), ti ? L"是" : L"否", (unsigned)session,
            uia ? L"是" : L"否", (unsigned)il);
        return b;
    }

    namespace Boot {

        static std::wstring errStage;
        static DWORD errCode = 0;

        static void SetError(const wchar_t* stage, DWORD code) {
            errStage = stage;
            errCode = code;
        }
        static void ClearError() {
            errStage.clear();
            errCode = 0;
        }
        static std::wstring LastBootError() {
            if (errStage.empty() && !errCode) return L"";
            wchar_t b[256];
            std::swprintf(b, 256, L"失败阶段=%s  Win32 错误码=0x%08X(%u)",
                errStage.empty() ? L"未记录" : errStage.c_str(),
                (unsigned)errCode, (unsigned)errCode);
            return b;
        }

        static bool HasFlag(std::wstring_view cmd, std::wstring_view flag) {
            return cmd.find(flag) != std::wstring_view::npos;
        }

        static std::wstring Arg(std::wstring_view cmd, std::wstring_view key) {
            size_t i = cmd.find(key);
            if (i == std::wstring_view::npos) return L"";
            i += key.size();
            size_t e = cmd.find(L' ', i);
            if (e == std::wstring_view::npos) e = cmd.size();
            return std::wstring(cmd.substr(i, e - i));
        }

        static bool IsElevated() {
            HANDLE t = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return false;
            TOKEN_ELEVATION e{};
            DWORD n = 0;
            BOOL ok = GetTokenInformation(t, TokenElevation, &e, sizeof(e), &n);
            CloseHandle(t);
            return ok && e.TokenIsElevated != 0;
        }

        static bool EnablePrivilege(HANDLE tok, const wchar_t* name) {
            TOKEN_PRIVILEGES tp{};
            tp.PrivilegeCount = 1;
            if (!LookupPrivilegeValueW(nullptr, name, &tp.Privileges[0].Luid))
                return false;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            SetLastError(ERROR_SUCCESS);
            AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
            return GetLastError() == ERROR_SUCCESS;
        }

        static DWORD CurrentSessionId() {
            DWORD sid = 0;
            ProcessIdToSessionId(GetCurrentProcessId(), &sid);
            return sid;
        }

        static DWORD FindWinlogonProcess() {
            const DWORD mySession = CurrentSessionId();
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) {
                SetError(L"CreateToolhelp32Snapshot", GetLastError());
                return 0;
            }
            PROCESSENTRY32W e{};
            e.dwSize = sizeof(e);
            DWORD pid = 0;
            if (Process32FirstW(snap, &e)) {
                do {
                    if (Lower(std::wstring(e.szExeFile)) == L"winlogon.exe") {
                        DWORD s = 0;
                        if (mySession && ProcessIdToSessionId(e.th32ProcessID, &s) &&
                            s == mySession) {
                            pid = e.th32ProcessID;
                            break;
                        }
                        if (!mySession) { pid = e.th32ProcessID; break; }
                    }
                } while (Process32NextW(snap, &e));
            }
            CloseHandle(snap);
            if (!pid) SetError(L"定位同会话Winlogon进程", ERROR_NOT_FOUND);
            else ClearError();
            return pid;
        }

        static HANDLE DuplicateSystemToken(DWORD pid) {
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!proc) { SetError(L"OpenProcess", GetLastError()); return nullptr; }
            HANDLE src = nullptr;
            BOOL ok = OpenProcessToken(proc, TOKEN_DUPLICATE | TOKEN_QUERY, &src);
            CloseHandle(proc);
            if (!ok) { SetError(L"OpenProcessToken", GetLastError()); return nullptr; }
            HANDLE out = nullptr;
            if (!DuplicateTokenEx(src, TOKEN_ALL_ACCESS, nullptr,
                SecurityDelegation, TokenPrimary, &out)) {
                SetError(L"DuplicateTokenEx", GetLastError());
                CloseHandle(src);
                return nullptr;
            }
            CloseHandle(src);
            ClearError();
            return out;
        }

        static bool RelaunchWithWinlogonToken(const std::wstring& interactiveSid) {
            ClearError();
            HANDLE self = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(),
                TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ADJUST_PRIVILEGES |
                TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY, &self)) {
                SetError(L"自身令牌", GetLastError());
                return false;
            }
            EnablePrivilege(self, SE_DEBUG_NAME);
            EnablePrivilege(self, SE_IMPERSONATE_NAME);
            EnablePrivilege(self, SE_ASSIGNPRIMARYTOKEN_NAME);
            EnablePrivilege(self, SE_INCREASE_QUOTA_NAME);

            DWORD pid = FindWinlogonProcess();
            if (!pid) { CloseHandle(self); return false; }
            HANDLE sysTok = DuplicateSystemToken(pid);
            if (!sysTok) { CloseHandle(self); return false; }

            HANDLE imp = nullptr;
            if (!DuplicateTokenEx(sysTok, TOKEN_ALL_ACCESS, nullptr,
                SecurityImpersonation, TokenImpersonation, &imp)) {
                SetError(L"复制模拟令牌", GetLastError());
                CloseHandle(sysTok); CloseHandle(self); return false;
            }
            if (!EnablePrivilege(imp, SE_TCB_NAME)) {
                SetError(L"启用SeTcbPrivilege", ERROR_PRIVILEGE_NOT_HELD);
                CloseHandle(imp); CloseHandle(sysTok); CloseHandle(self); return false;
            }
            EnablePrivilege(imp, SE_ASSIGNPRIMARYTOKEN_NAME);
            EnablePrivilege(imp, SE_INCREASE_QUOTA_NAME);
            EnablePrivilege(imp, SE_IMPERSONATE_NAME);
            if (!ImpersonateLoggedOnUser(imp)) {
                SetError(L"ImpersonateLoggedOnUser", GetLastError());
                CloseHandle(imp); CloseHandle(sysTok); CloseHandle(self); return false;
            }

            DWORD session = 0, need = 0;
            HANDLE q = nullptr;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &q)) {
                GetTokenInformation(q, TokenSessionId, &session, sizeof(session), &need);
                CloseHandle(q);
            }
            if (!session) session = CurrentSessionId();
            DWORD one = 1;
            BOOL s1 = SetTokenInformation(sysTok, TokenSessionId, &session, sizeof(session));
            DWORD e1 = GetLastError();
            BOOL s2 = SetTokenInformation(sysTok, TokenUIAccess, &one, sizeof(one));
            DWORD e2 = GetLastError();
            if (!s1 || !s2) {
                SetError(s1 ? L"TokenUIAccess" : L"TokenSessionId", s1 ? e2 : e1);
                RevertToSelf();
                CloseHandle(imp);
                CloseHandle(sysTok); CloseHandle(self); return false;
            }

            void* env = nullptr;
            BOOL envOk = CreateEnvironmentBlock(&env, sysTok, FALSE);
            WCHAR exe[MAX_PATH * 2]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
            std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --boot-uia";
            if (!interactiveSid.empty()) cmd += L" --sid=" + interactiveSid;
            std::vector<wchar_t> cb(cmd.begin(), cmd.end());
            cb.push_back(0);

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.lpDesktop = const_cast<LPWSTR>(L"WinSta0\\Default");
            PROCESS_INFORMATION pi{};
            BOOL created = CreateProcessAsUserW(sysTok, nullptr, cb.data(),
                nullptr, nullptr, FALSE,
                envOk ? CREATE_UNICODE_ENVIRONMENT : 0u,
                envOk ? env : nullptr, nullptr, &si, &pi);
            DWORD createdError = created ? 0u : GetLastError();
            RevertToSelf();
            CloseHandle(imp);
            if (envOk) DestroyEnvironmentBlock(env);
            if (created) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            else {
                SetError(L"CreateProcessAsUser", createdError);
            }
            CloseHandle(sysTok);
            CloseHandle(self);
            return created ? true : false;
        }

        static bool SelfElevate(std::wstring_view extra) {
            WCHAR exe[MAX_PATH * 2]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
            std::wstring args = L"--boot-elevated";
            if (!extra.empty()) { args += L" "; args += extra; }
            HINSTANCE r = ShellExecuteW(nullptr, L"runas", exe,
                args.c_str(), nullptr, SW_SHOWNORMAL);
            return reinterpret_cast<INT_PTR>(r) > 32;
        }

    }

    static Conv RegPathToNtPath(std::wstring p, std::wstring_view userSid = {}) {
        if (p.empty()) return BadC(L"路径为空");
        if (Starts(p, L"\\Registry\\")) return OkC(p);
        for (auto prefix : { L"Computer\\", L"My Computer\\" })
            if (Starts(p, prefix)) { p = p.substr(std::wstring_view(prefix).size()); break; }
        static const struct { const wchar_t* alias; const wchar_t* nt; } roots[] = {
            { L"HKEY_LOCAL_MACHINE", L"\\Registry\\Machine" },
            { L"HKLM",               L"\\Registry\\Machine" },
            { L"HKEY_USERS",         L"\\Registry\\User" },
            { L"HKU",                L"\\Registry\\User" }
        };
        for (const auto& r : roots) {
            std::wstring_view a(r.alias);
            if (Starts(p, a) && (p.size() == a.size() || p[a.size()] == L'\\')) {
                std::wstring tail = p.size() > a.size() ? p.substr(a.size()) : L"";
                return OkC(std::wstring(r.nt) + tail);
            }
        }
        for (auto alias : { L"HKEY_CURRENT_USER", L"HKCU" }) {
            std::wstring_view a(alias);
            if (Starts(p, a) && (p.size() == a.size() || p[a.size()] == L'\\')) {
                std::optional<std::wstring> sid;
                if (!userSid.empty()) sid = std::wstring(userSid);
                else sid = CurrentUserSid();
                if (!sid) return BadC(L"当前用户 SID 查询失败");
                std::wstring tail = p.size() > a.size() ? p.substr(a.size()) : L"";
                return OkC(L"\\Registry\\User\\" + *sid + tail);
            }
        }
        std::wstring low = Lower(p);
        if (Starts(low, L"hkey_classes_root") || Starts(low, L"hkcr")) return BadC(L"HKCR 是合并视图");
        if (Starts(low, L"hkey_current_config") || Starts(low, L"hkcc")) return BadC(L"HKCC 是动态映射");
        if (Starts(low, L"hkey_performance_data")) return BadC(L"不是真实注册表键");
        return BadC(L"无法识别的根键");
    }

    struct KUnicodeString {
        std::uint16_t Length;
        std::uint16_t MaximumLength;
        void* Buffer;
    };

    struct KSystemProcessInformation {
        std::uint32_t NextEntryOffset;
        std::uint32_t NumberOfThreads;
        std::int64_t  Reserved[3];
        std::int64_t  CreateTime;
        std::int64_t  UserTime;
        std::int64_t  KernelTime;
        KUnicodeString ImageName;
        std::int32_t  BasePriority;
        void* UniqueProcessId;
        void* InheritedFromUniqueProcessId;
        std::uint32_t HandleCount;
        std::uint32_t SessionId;
        std::uint64_t PageDirectoryBase;
        std::uint64_t PeakVirtualSize;
        std::uint64_t VirtualSize;
        std::uint32_t Reserved2;
        std::uint64_t PrivatePageCount;
        std::uint64_t PeakWorkingSetSize;
        std::uint64_t WorkingSetSize;
        std::uint64_t QuotaPagedPoolUsage;
        std::uint64_t QuotaNonPagedPoolUsage;
        std::uint64_t PagefileUsage;
        std::uint64_t PeakPagefileUsage;
    };

    static_assert(offsetof(KSystemProcessInformation, ImageName) == 56, "spi_imagename");
    static_assert(offsetof(KSystemProcessInformation, UniqueProcessId) == 80, "spi_pid");
    static_assert(offsetof(KSystemProcessInformation, InheritedFromUniqueProcessId) == 88, "spi_ppid");
    static_assert(offsetof(KSystemProcessInformation, HandleCount) == 96, "spi_handles");
    static_assert(offsetof(KSystemProcessInformation, WorkingSetSize) == 152, "spi_ws");

    struct SysProc {
        kc_u32 pid = 0;
        kc_u32 parent = 0;
        std::uint64_t createTime = 0;
        std::wstring image;
        std::uint64_t ticks = 0;
        std::uint64_t ws = 0;
        std::uint64_t peakWs = 0;
        std::uint64_t virtualSize = 0;
        std::uint64_t privateBytes = 0;
        std::uint64_t pagefile = 0;
        std::uint64_t quotaPaged = 0;
        std::uint64_t quotaNonPaged = 0;
        std::uint32_t handles = 0;
        std::uint32_t threads = 0;
        std::uint32_t session = 0;
    };

    static std::map<kc_u32, SysProc> QuerySystemProcesses() {
        std::map<kc_u32, SysProc> out;
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return out;
        using Fn = std::int32_t(NTAPI*)(std::uint32_t, void*, std::uint32_t, std::uint32_t*);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
        if (!fn) return out;
        std::vector<std::uint8_t> buf(64 * 1024);
        std::uint32_t need = 0;
        const std::int32_t mismatch = (std::int32_t)0xC0000004u;
        bool ok = false;
        for (int i = 0; i < 6; ++i) {
            std::int32_t st = fn(5, buf.data(), (std::uint32_t)buf.size(), &need);
            if (st == 0) { ok = true; break; }
            std::size_t next = (need > buf.size()) ? (std::size_t)need + 4096 : buf.size() * 2;
            if (next > 32u * 1024u * 1024u) break;
            buf.assign(next, 0);
            if (st != mismatch) break;
        }
        if (!ok) return out;
        const std::uint8_t* base = buf.data();
        const std::uint8_t* end = base + buf.size();
        const std::uint8_t* cur = base;
        for (;;) {
            if (cur + sizeof(KSystemProcessInformation) > end) break;
            const auto* p = reinterpret_cast<const KSystemProcessInformation*>(cur);
            SysProc sp;
            sp.pid = (kc_u32)(std::uintptr_t)p->UniqueProcessId;
            sp.parent = (kc_u32)(std::uintptr_t)p->InheritedFromUniqueProcessId;
            sp.createTime = (std::uint64_t)p->CreateTime;
            sp.ticks = (std::uint64_t)p->KernelTime + (std::uint64_t)p->UserTime;
            sp.ws = p->WorkingSetSize;
            sp.peakWs = p->PeakWorkingSetSize;
            sp.virtualSize = p->VirtualSize;
            sp.privateBytes = p->PrivatePageCount;
            sp.pagefile = p->PagefileUsage;
            sp.quotaPaged = p->QuotaPagedPoolUsage;
            sp.quotaNonPaged = p->QuotaNonPagedPoolUsage;
            sp.handles = p->HandleCount;
            sp.threads = p->NumberOfThreads;
            sp.session = p->SessionId;
            if (p->ImageName.Buffer && p->ImageName.Length >= 2) {
                const auto* u = reinterpret_cast<const std::uint16_t*>(p->ImageName.Buffer);
                std::size_t n = (std::size_t)p->ImageName.Length / 2;
                if (reinterpret_cast<const std::uint8_t*>(u + n) <= end) {
                    for (std::size_t k = 0; k < n; ++k)
                        sp.image.push_back((wchar_t)u[k]);
                }
            }
            out[sp.pid] = std::move(sp);
            if (p->NextEntryOffset == 0) break;
            if (p->NextEntryOffset < sizeof(KSystemProcessInformation)) break;
            cur += p->NextEntryOffset;
        }
        return out;
    }

    static void SampleGlobal(AppState& st) {
        FILETIME idle{}, kern{}, user{};
        if (GetSystemTimes(&idle, &kern, &user)) {
            auto u64 = [](const FILETIME& f) -> std::uint64_t {
                return ((std::uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime;
                };
            std::uint64_t k = u64(kern), u = u64(user), i = u64(idle);
            std::uint64_t total = k + u;
            std::uint64_t busy = (total > i) ? (total - i) : 0;
            if (st.lastTotal && total > st.lastTotal && busy >= st.lastBusy) {
                std::uint64_t dTotal = total - st.lastTotal;
                std::uint64_t dBusy = busy - st.lastBusy;
                st.sysDelta = dBusy;
                st.cpuPct = (dTotal > 0)
                    ? (double)dBusy * 100.0 / (double)dTotal : 0.0;
            }
            else st.sysDelta = 0;
            st.lastTotal = total;
            st.lastBusy = busy;
        }
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) st.memPct = ms.dwMemoryLoad;
        wchar_t t[160];
        std::swprintf(t, 160, L"CPU 使用率: %d%%   内存占用率: %u%%",
            (int)(st.cpuPct + 0.5), (unsigned)st.memPct);
        st.hudText = t;
    }

    class Device {
    public:
        Device() = default;
        ~Device() { Close(); }
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        void SetPath(const std::wstring& p) {
            if (p == path_) return;
            Close();
            path_ = p.empty() ? kDefaultDevice : p;
        }
        bool Connected() const noexcept { return h_ && h_ != INVALID_HANDLE_VALUE; }
        unsigned long LastWin32() const noexcept { return (unsigned long)lastWin32_; }

        bool Open() {
            if (Connected()) return true;
            h_ = CreateFileW(path_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h_ == INVALID_HANDLE_VALUE) { lastWin32_ = GetLastError(); return false; }
            lastWin32_ = 0;
            return true;
        }
        void Close() {
            if (Connected()) CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
        std::optional<KC_REPLY> Call(kc_u32 code, const KC_REQUEST& req) {
            if (!Open()) return std::nullopt;
            KC_REPLY rep{};
            DWORD got = 0;
            BOOL ok = DeviceIoControl(h_, (DWORD)code,
                const_cast<KC_REQUEST*>(&req), (DWORD)sizeof(req),
                &rep, (DWORD)sizeof(rep), &got, nullptr);
            if (!ok) {
                lastWin32_ = GetLastError();
                Close();
                return std::nullopt;
            }
            return rep;
        }
    private:
        HANDLE h_ = INVALID_HANDLE_VALUE;
        std::wstring path_ = kDefaultDevice;
        DWORD lastWin32_ = 0;
    };

    static Device g_dev;
    static example::ExampleConfirmWindow g_confirm;

    static KC_REQUEST MakeReq(kc_u32 code, kc_u64 value = 0, kc_u32 flags = 0) {
        KC_REQUEST r{};
        r.Command = code;
        r.Value = value;
        r.Flags = flags;
        return r;
    }

    static void PutPath(KC_REQUEST& r, bool second, std::wstring_view s) {
        if (second) PackPath(s, r.PathB, KC_MAX_PATH, r.PathBLength);
        else        PackPath(s, r.PathA, KC_MAX_PATH, r.PathALength);
    }

    template <typename T>
    static T ReadPayload(const KC_REPLY& rep) {
        T t{};
        std::memcpy(&t, rep.Payload, (std::min)((size_t)rep.Bytes, sizeof(T)));
        return t;
    }

    static std::optional<kc_u64> ParseNum(const std::wstring& s) {
        try {
            size_t p = 0;
            unsigned long long v = std::stoull(s, &p, 0);
            if (p != s.size()) return std::nullopt;
            return (kc_u64)v;
        }
        catch (...) { return std::nullopt; }
    }

    static void AddLog(AppState& st, const std::wstring& s) {
        st.logs.push_back(s);
        if (st.logs.size() > 400)
            st.logs.erase(st.logs.begin(), st.logs.begin() + (st.logs.size() - 400));
        st.lastAction = s;
    }

    static bool Good(AppState& st, const std::optional<KC_REPLY>& rep, const wchar_t* what) {
        if (!rep) {
            AddLog(st, std::wstring(L"失败 ") + what + L" 设备调用失败 win32=" +
                std::to_wstring(g_dev.LastWin32()));
            return false;
        }
        if (rep->Status != KC_OK) {
            AddLog(st, std::wstring(L"失败 ") + what + L" " + Hex(rep->Status) +
                L" " + StatusName(rep->Status));
            return false;
        }
        return true;
    }

    static void SyncAccent(AppState& st, example::U32 rgb) {
        st.accentR = (float)((rgb >> 16) & 0xFF) / 255.f;
        st.accentG = (float)((rgb >> 8) & 0xFF) / 255.f;
        st.accentB = (float)(rgb & 0xFF) / 255.f;
    }
    static void SyncBackground(AppState& st, example::U32 rgb) {
        st.bgR = (float)((rgb >> 16) & 0xFF) / 255.f;
        st.bgG = (float)((rgb >> 8) & 0xFF) / 255.f;
        st.bgB = (float)(rgb & 0xFF) / 255.f;
    }
    static example::U32 ComposeAccent(const AppState& st) {
        int r = (std::max)(0, (std::min)(255, (int)(st.accentR * 255.f + 0.5f)));
        int g = (std::max)(0, (std::min)(255, (int)(st.accentG * 255.f + 0.5f)));
        int b = (std::max)(0, (std::min)(255, (int)(st.accentB * 255.f + 0.5f)));
        return (example::U32)((r << 16) | (g << 8) | b);
    }
    static example::U32 ComposeBackground(const AppState& st) {
        int r = (std::max)(0, (std::min)(255, (int)(st.bgR * 255.f + 0.5f)));
        int g = (std::max)(0, (std::min)(255, (int)(st.bgG * 255.f + 0.5f)));
        int b = (std::max)(0, (std::min)(255, (int)(st.bgB * 255.f + 0.5f)));
        return (example::U32)((r << 16) | (g << 8) | b);
    }
    static void ApplyTheme(int idx, AppState& st) {
        const ThemePreset& t = kThemes[idx];
        example::color::BgTop = t.bgTop;           example::color::BgBottom = t.bgBottom;
        example::color::Panel = t.panel;           example::color::PanelStroke = t.panelStroke;
        example::color::Control = t.control;       example::color::ControlHover = t.controlHover;
        example::color::ControlPress = t.controlPress; example::color::Stroke = t.stroke;
        example::color::Text = t.text;             example::color::TextMuted = t.textMuted;
        example::color::TextDim = t.textDim;       example::color::RowAlt = t.rowAlt;
        example::color::Header = t.header;         example::color::NavBg = t.navBg;
        example::color::NavActive = t.navActive;   example::color::OnAccent = t.onAccent;
        example::color::Accent = t.accent;         example::color::AccentPress = t.accentPress;
        SyncAccent(st, t.accent);
        SyncBackground(st, t.bgBottom);
        st.theme = idx;
    }
    static void ResetAll(AppState& st) {
        example::ExampleResetStyle();
        st.animEnabled = true;
        st.fadeSec = 0.18f;
        SyncAccent(st, 0x60CDFF);
        SyncBackground(st, 0x202124);
        st.theme = 0;
        st.lastAction = L"恢复默认值";
    }

    static kc_u32 EventCap(const KC_REPLY& rep) {
        const size_t head = offsetof(KC_EVENT_BATCH, Events);
        return rep.Bytes >= head
            ? (kc_u32)((rep.Bytes - head) / sizeof(KC_EVENT)) : 0u;
    }
    static kc_u32 PageCap(const KC_REPLY& rep) {
        const size_t head = offsetof(KC_ENUM_PAGE, Items);
        return rep.Bytes >= head
            ? (kc_u32)((rep.Bytes - head) / sizeof(KC_ENUM_ITEM)) : 0u;
    }

    static std::map<kc_u32, kc_u32> QueryDriverProtection(AppState& st) {
        std::map<kc_u32, kc_u32> out;
        kc_u32 flags = st.strictEnum ? (kc_u32)KC_ENUM_STRICT_LIVENESS : 0u;
        auto open = g_dev.Call(IOCTL_KC_ENUM_OPEN,
            MakeReq(IOCTL_KC_ENUM_OPEN, KC_ENUM_KIND_PROC, flags));
        if (!open || open->Status != KC_OK) return out;
        KC_ENUM_OPEN_RESULT o = ReadPayload<KC_ENUM_OPEN_RESULT>(*open);
        kc_u32 session = o.Session;
        kc_u32 cursor = 0;
        for (;;) {
            auto rep = g_dev.Call(IOCTL_KC_ENUM_NEXT,
                MakeReq(IOCTL_KC_ENUM_NEXT, session, cursor));
            if (!rep || rep->Status != KC_OK) break;
            KC_ENUM_PAGE p = ReadPayload<KC_ENUM_PAGE>(*rep);
            kc_u32 n = (std::min)(p.Count, PageCap(*rep));
            for (kc_u32 i = 0; i < n; ++i) out[p.Items[i].Id] = p.Items[i].Flags;
            cursor += p.Count;
            if (!p.More || p.Count == 0 || out.size() > 8000) break;
        }
        (void)g_dev.Call(IOCTL_KC_ENUM_CLOSE, MakeReq(IOCTL_KC_ENUM_CLOSE, session));
        return out;
    }

    static std::wstring BaseName(const std::wstring& p) {
        if (p.empty()) return L"";
        size_t i = p.find_last_of(L"\\/");
        return i == std::wstring::npos ? p : p.substr(i + 1);
    }

    static void RebuildOrder(AppState& st) {
        st.order.clear();
        auto validParent = [&](kc_u32 pp, kc_u32 self) -> bool {
            if (!pp || pp == self) return false;
            auto p = st.nodes.find(pp);
            return p != st.nodes.end() && p->second.state != 2;
            };
        for (auto& kv : st.nodes) kv.second.hasChild = false;
        for (auto& kv : st.nodes) {
            const kc_u32 pp = kv.second.parent;
            if (validParent(pp, kv.first)) st.nodes[pp].hasChild = true;
        }
        auto key = [&](kc_u32 pid) -> std::wstring {
            auto it = st.nodes.find(pid);
            if (it == st.nodes.end()) return L"";
            switch (st.sortCol) {
            case 1: {
                wchar_t b[32]; std::swprintf(b, 32, L"%010u", pid); return b;
            }
            case 2: {
                wchar_t b[32]; std::swprintf(b, 32, L"%018.2f", it->second.cpu); return b;
            }
            case 3: {
                wchar_t b[48]; std::swprintf(b, 48, L"%020llu",
                    (unsigned long long)it->second.mem); return b;
            }
            case 4: return it->second.company;
            case 5: return it->second.desc;
            case 6: return it->second.path;
            default: return Lower(it->second.name);
            }
            };
        std::vector<kc_u32> roots;
        for (const auto& kv : st.nodes)
            if (!validParent(kv.second.parent, kv.first)) roots.push_back(kv.first);
        std::sort(roots.begin(), roots.end(), [&](kc_u32 a, kc_u32 b) {
            std::wstring ka = key(a), kb = key(b);
            return st.sortAsc ? (ka < kb) : (ka > kb);
            });
        std::set<kc_u32> visited;
        std::function<void(kc_u32, int)> walk = [&](kc_u32 pid, int depth) {
            if (visited.count(pid) || !st.nodes.count(pid)) return;
            visited.insert(pid);
            st.order.push_back({ pid, depth });
            if (st.collapsed.count(pid)) return;
            std::vector<kc_u32> kids;
            for (const auto& kv : st.nodes)
                if (kv.second.parent == pid && kv.first != pid &&
                    validParent(kv.second.parent, kv.first) &&
                    !visited.count(kv.first)) kids.push_back(kv.first);
            std::sort(kids.begin(), kids.end(), [&](kc_u32 a, kc_u32 b) {
                std::wstring ka = key(a), kb = key(b);
                return st.sortAsc ? (ka < kb) : (ka > kb);
                });
            for (kc_u32 k : kids)
                if (depth < 24 && !visited.count(k)) walk(k, depth + 1);
            };
        for (kc_u32 r : roots) walk(r, 0);
        for (const auto& kv : st.nodes)
            if (!visited.count(kv.first)) {
                visited.insert(kv.first);
                st.order.push_back({ kv.first, 0 });
            }
        if (!st.filter.empty()) {
            std::wstring f = Lower(st.filter);
            std::vector<std::pair<kc_u32, int>> keep;
            for (const auto& e : st.order) {
                auto it = st.nodes.find(e.first);
                if (it == st.nodes.end()) continue;
                if (Lower(it->second.name).find(f) != std::wstring::npos ||
                    Lower(it->second.path).find(f) != std::wstring::npos ||
                    std::to_wstring(e.first).find(f) != std::wstring::npos)
                    keep.push_back({ e.first, 0 });
            }
            st.order = std::move(keep);
        }
    }

    static void RefreshProcs(AppState& st) {
        std::map<kc_u32, SysProc> sys = QuerySystemProcesses();
        std::map<kc_u32, kc_u32> prot = QueryDriverProtection(st);

        for (const auto& kv : sys) {
            const kc_u32 pid = kv.first;
            const SysProc& sp = kv.second;
            auto it = st.nodes.find(pid);
            if (it == st.nodes.end() ||
                (it->second.createTime && sp.createTime &&
                    it->second.createTime != sp.createTime)) {
                const bool reused = (it != st.nodes.end());
                if (reused) {
                    st.verCache.erase(pid);
                    st.collapsed.erase(pid);
                }
                ProcNode n;
                n.pid = pid;
                n.state = st.primed ? 1 : 0;
                n.age = 0;
                n.lastCpuTicks = sp.ticks;
                n.createTime = sp.createTime;
                st.nodes[pid] = n;
                it = st.nodes.find(pid);
            }
            else if (it->second.state == 2) {
                it->second.state = 1;
                it->second.age = 0;
                it->second.deadAge = 0;
                it->second.lastCpuTicks = sp.ticks;
                st.verCache.erase(pid);
            }
            else if (it->second.state == 1) {
                ++it->second.age;
                if (it->second.age >= 5) it->second.state = 0;
            }
            ProcNode& n = it->second;
            n.createTime = sp.createTime;
            n.parent = sp.parent;
            n.raw = prot.count(pid) ? prot[pid] : 0u;
            std::wstring winPath = NtToWin32Path(sp.image);
            n.path = winPath;
            n.name = BaseName(sp.image);
            if (n.name.empty()) n.name = L"System";
            n.mem = sp.ws;
            n.peakWs = sp.peakWs;
            n.virtualSize = sp.virtualSize;
            n.privateBytes = sp.privateBytes;
            n.pagefile = sp.pagefile;
            n.quotaPaged = sp.quotaPaged;
            n.quotaNonPaged = sp.quotaNonPaged;
            n.handles = sp.handles;
            n.threads = sp.threads;
            n.session = sp.session;
            std::uint64_t delta = (sp.ticks > n.lastCpuTicks)
                ? (sp.ticks - n.lastCpuTicks) : 0;
            n.lastCpuTicks = sp.ticks;
            n.cpu = (pid == 0) ? (100.0 - st.cpuPct)
                : ((st.sysDelta > 0)
                    ? (double)delta * 100.0 / (double)st.sysDelta : 0.0);
            auto vc = st.verCache.find(pid);
            if (vc == st.verCache.end()) {
                VerInfo v = ReadVersionInfo(winPath);
                st.verCache[pid] = v;
                vc = st.verCache.find(pid);
            }
            n.company = vc->second.company;
            n.desc = vc->second.desc;
        }

        std::vector<kc_u32> drop;
        for (auto& kv : st.nodes) {
            if (sys.count(kv.first)) continue;
            if (kv.second.state != 2) {
                kv.second.state = 2;
                kv.second.deadAge = 0;
            }
            else {
                ++kv.second.deadAge;
                if (kv.second.deadAge >= 12) drop.push_back(kv.first);
            }
            if (kv.second.state == 2) {
                bool replaced = false;
                for (const auto& live : sys) {
                    auto li = st.nodes.find(live.first);
                    if (li == st.nodes.end() || li->second.state == 2 ||
                        li->first == kv.first || kv.second.name.empty()) continue;
                    if (li->second.name == kv.second.name &&
                        li->second.path == kv.second.path) {
                        replaced = true;
                        break;
                    }
                }
                if (replaced &&
                    std::find(drop.begin(), drop.end(), kv.first) == drop.end())
                    drop.push_back(kv.first);
            }
        }
        for (kc_u32 p : drop) {
            st.nodes.erase(p);
            st.verCache.erase(p);
            st.collapsed.erase(p);
        }
        st.primed = true;
        RebuildOrder(st);
        wchar_t t[160];
        std::swprintf(t, 160, L"CPU 使用率: %d%%   内存占用率: %u%%   进程数: %u",
            (int)(st.cpuPct + 0.5), (unsigned)st.memPct, (unsigned)sys.size());
        st.hudText = t;
    }

    static void RefreshBottom(AppState& st) {
        st.bottomRows.clear();
        auto it = st.nodes.find(st.selectedPid);
        if (st.bottomTab == 0) {
            st.bottomHeaders = { L"模块名", L"映像基址", L"映像大小", L"模块路径", L"公司名", L"文件描述" };
            if (it == st.nodes.end() || it->second.state == 2) {
                st.bottomRows.push_back({ L"-", L"-", L"-", L"未选定进程", L"-", L"-" });
                return;
            }
            HANDLE snap = CreateToolhelp32Snapshot(
                TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)st.selectedPid);
            if (snap == INVALID_HANDLE_VALUE) {
                st.bottomRows.push_back({ L"-", L"-", L"-",
                    L"模块枚举失败（受保护进程或权限不足）", L"-", L"-" });
                return;
            }
            MODULEENTRY32W me{};
            me.dwSize = sizeof(me);
            if (Module32FirstW(snap, &me)) {
                do {
                    VerInfo v = ReadVersionInfo(me.szExePath);
                    st.bottomRows.push_back({
                        me.szModule,
                        Hex64((std::uint64_t)(std::uintptr_t)me.modBaseAddr),
                        MemSize(me.modBaseSize),
                        me.szExePath, v.company, v.desc
                        });
                } while (Module32NextW(snap, &me) && st.bottomRows.size() < 200);
            }
            else {
                st.bottomRows.push_back({ L"-", L"-", L"-",
                    L"模块枚举失败（受保护进程或权限不足）", L"-", L"-" });
            }
            CloseHandle(snap);
        }
        else if (st.bottomTab == 1) {
            st.bottomHeaders = { L"类型", L"PID", L"操作", L"状态", L"时间", L"详情" };
            if (it != st.nodes.end())
                st.bottomRows.push_back({ L"handles", std::to_wstring(st.selectedPid),
                    L"句柄计数", std::to_wstring(it->second.handles), L"-", L"-" });
            for (const auto& r : st.eventRows)
                if (r.size() >= 6 && r[0] == L"handle" &&
                    r[1] == std::to_wstring(st.selectedPid)) st.bottomRows.push_back(r);
            if (st.bottomRows.empty())
                st.bottomRows.push_back({ L"-", L"-", L"-", L"未选定进程", L"-", L"-" });
        }
        else {
            st.bottomHeaders = { L"项目", L"值" };
            if (it == st.nodes.end()) {
                st.bottomRows.push_back({ L"未选定进程", L"-", L"-", L"-", L"-", L"-" });
                return;
            }
            const ProcNode& n = it->second;
            st.bottomRows.push_back({ L"工作集", MemSize(n.mem), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"峰值工作集", MemSize(n.peakWs), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"虚拟大小", MemSize(n.virtualSize), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"私有字节", MemSize(n.privateBytes), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"页面文件", MemSize(n.pagefile), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"分页池配额", MemSize(n.quotaPaged), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"非分页池配额", MemSize(n.quotaNonPaged), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"句柄数", std::to_wstring(n.handles), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"线程数", std::to_wstring(n.threads), L"-", L"-", L"-", L"-" });
            st.bottomRows.push_back({ L"会话", std::to_wstring(n.session), L"-", L"-", L"-", L"-" });
        }
        if (st.bottomOffset > 0) {
            std::size_t k = (std::size_t)st.bottomOffset;
            if (k >= st.bottomRows.size()) st.bottomOffset = 0;
            else st.bottomRows.erase(st.bottomRows.begin(),
                st.bottomRows.begin() + k);
        }
    }

    static void DoProtect(AppState& st, const wchar_t* what, bool logOk) {
        auto pid = ParseNum(st.pidInput);
        if (!pid) { AddLog(st, L"失败 进程 ID 无效"); return; }
        auto rep = g_dev.Call(IOCTL_KC_PROTECT, MakeReq(IOCTL_KC_PROTECT, *pid));
        if (!Good(st, rep, what)) return;
        KC_PROTECT_INFO x = ReadPayload<KC_PROTECT_INFO>(*rep);
        std::wstring s = L"pid=" + std::to_wstring(x.Pid) +
            L" raw=" + Hex(x.Raw) + L" type=" + std::to_wstring((unsigned)x.Type) +
            L" audit=" + std::to_wstring((unsigned)x.Audit) +
            L" signer=" + std::to_wstring((unsigned)x.Signer) +
            L" sn=" + SignerName((unsigned)x.Signer) +
            L" off=" + Hex(x.ProtectionOffset);
        st.resultText = s;
        if (logOk) AddLog(st, L"完成 " + std::wstring(what) + L" " + s);
    }

    static void DoCritical(AppState& st, kc_u32 op, const wchar_t* name, bool logOk) {
        auto pid = ParseNum(st.pidInput);
        if (!pid) { AddLog(st, L"失败 进程 ID 无效"); return; }
        kc_u32 flags = op;
        if (op == KC_CRIT_OP_CLEAR) flags |= KC_CRIT_ALLOW_CLEAR;
        auto rep = g_dev.Call(IOCTL_KC_CRITICAL, MakeReq(IOCTL_KC_CRITICAL, *pid, flags));
        if (!Good(st, rep, name)) return;
        KC_CRITICAL_RESULT x = ReadPayload<KC_CRITICAL_RESULT>(*rep);
        std::wstring s = L"pid=" + std::to_wstring(x.Pid) +
            L" op=" + name + L" before=" + std::to_wstring(x.Before) +
            L" after=" + std::to_wstring(x.After) +
            L" restored=" + std::to_wstring(x.Restored);
        st.resultText = s;
        if (logOk) AddLog(st, L"完成 " + std::wstring(name) + L" " + s);
    }

    static void DoCriticalToggle(AppState& st) {
        auto pid = ParseNum(st.pidInput);
        if (!pid) { AddLog(st, L"失败 进程 ID 无效"); return; }
        auto q = g_dev.Call(IOCTL_KC_CRITICAL,
            MakeReq(IOCTL_KC_CRITICAL, *pid, KC_CRIT_OP_QUERY));
        if (!q || q->Status != KC_OK) {
            AddLog(st, L"失败 BreakOnTermination 查询失败，未执行修改");
            return;
        }
        KC_CRITICAL_RESULT cur = ReadPayload<KC_CRITICAL_RESULT>(*q);
        const bool isCritical = cur.Before != 0;
        const kc_u32 op = isCritical ? (kc_u32)KC_CRIT_OP_CLEAR : (kc_u32)KC_CRIT_OP_SET;
        kc_u32 flags = op;
        if (op == KC_CRIT_OP_CLEAR) flags |= KC_CRIT_ALLOW_CLEAR;
        const wchar_t* name = isCritical ? L"清除 BreakOnTermination" : L"设置 BreakOnTermination";
        auto rep = g_dev.Call(IOCTL_KC_CRITICAL,
            MakeReq(IOCTL_KC_CRITICAL, *pid, flags));
        if (!Good(st, rep, name)) return;
        KC_CRITICAL_RESULT x = ReadPayload<KC_CRITICAL_RESULT>(*rep);
        std::wstring s = L"pid=" + std::to_wstring(x.Pid) +
            L" op=" + std::wstring(name) +
            L" before=" + std::to_wstring(x.Before) +
            L" after=" + std::to_wstring(x.After) +
            L" restored=" + std::to_wstring(x.Restored);
        st.resultText = s;
        AddLog(st, L"完成 " + std::wstring(name) + L" " + s);
    }

    static void DoStrip(AppState& st) {
        auto pid = ParseNum(st.pidInput);
        if (!pid) { AddLog(st, L"失败 进程 ID 无效"); return; }
        kc_u32 flags = st.allowCritical ? (kc_u32)KC_STRIP_ALLOW_CRITICAL : 0u;
        auto rep = g_dev.Call(IOCTL_KC_STRIP_PROTECTION,
            MakeReq(IOCTL_KC_STRIP_PROTECTION, *pid, flags));
        if (!Good(st, rep, L"剥离/附加PPL")) return;
        KC_STRIP_RESULT x = ReadPayload<KC_STRIP_RESULT>(*rep);
        std::wstring s = L"pid=" + std::to_wstring(x.Pid) +
            L" off=" + Hex(x.ProtectionOffset) +
            L" before=" + Hex(x.ProtectionBefore) +
            L" after=" + Hex(x.ProtectionAfter) +
            L" critical=" + std::to_wstring(x.WasCritical) +
            L" restored=" + std::to_wstring(x.Restored) +
            L" clear=" + std::to_wstring(x.AlreadyClear);
        st.resultText = s;
        if (x.AlreadyClear) {
            st.resultText += L"  目标未处于 PPL 保护状态；驱动未提供 PPL 设置接口，未执行修改";
            AddLog(st, L"失败 PPL 保护设置未实现（驱动仅提供剥离接口），未执行修改");
            return;
        }
        AddLog(st, L"完成 PPL 保护剥离 " + s);
    }

    static void DoKill(AppState& st) {
        auto pid = ParseNum(st.pidInput);
        if (!pid) { AddLog(st, L"失败 进程 ID 无效"); return; }
        kc_u32 flags = st.allowCritical ? (kc_u32)KC_TERM_ALLOW_CRITICAL : 0u;
        auto rep = g_dev.Call(IOCTL_KC_KILL, MakeReq(IOCTL_KC_KILL, *pid, flags));
        if (!Good(st, rep, L"终止进程")) return;
        KC_TERM_RESULT x = ReadPayload<KC_TERM_RESULT>(*rep);
        std::wstring s = L"pid=" + std::to_wstring(x.Pid) +
            L" critical=" + std::to_wstring(x.WasCritical) +
            L" refused=" + std::to_wstring(x.Refused);
        st.resultText = s;
        AddLog(st, L"完成 终止进程 " + s);
    }

    static void Precheck(AppState& st, const wchar_t* what) {
        auto pid = ParseNum(st.pidInput);
        if (!pid) { st.pendingText = L"进程 ID 无效，未执行"; return; }
        std::wstring info = std::wstring(what) + L" PID=" + std::to_wstring(*pid);
        auto a = g_dev.Call(IOCTL_KC_PROTECT, MakeReq(IOCTL_KC_PROTECT, *pid));
        if (a && a->Status == KC_OK) {
            KC_PROTECT_INFO x = ReadPayload<KC_PROTECT_INFO>(*a);
            info += L"  raw=" + Hex(x.Raw) + L"  sn=" + SignerName((unsigned)x.Signer);
        }
        else info += L"  保护级别查询失败";
        auto b = g_dev.Call(IOCTL_KC_CRITICAL,
            MakeReq(IOCTL_KC_CRITICAL, *pid, KC_CRIT_OP_QUERY));
        if (b && b->Status == KC_OK) {
            KC_CRITICAL_RESULT x = ReadPayload<KC_CRITICAL_RESULT>(*b);
            info += L"  BreakOnTermination=" + std::to_wstring(x.Before);
            if (x.Before) info += L"  警告：目标为关键进程";
        }
        else info += L"  BreakOnTermination 查询失败";
        st.pendingText = info;
    }

    static void BeginPending(AppState& st, Pending p, const wchar_t* what) {
        if (st.selectedPid) st.pidInput = std::to_wstring(st.selectedPid);
        st.page = 0;
        st.pending = p;
        Precheck(st, what);
    }

    static void DoDel(AppState& st) {
        Conv c = Win32ToNtPath(st.fileA);
        if (!c.ok) { AddLog(st, L"失败 路径=" + c.err); return; }
        KC_REQUEST r = MakeReq(IOCTL_KC_DEL);
        PutPath(r, false, c.nt);
        auto rep = g_dev.Call(IOCTL_KC_DEL, r);
        if (!Good(st, rep, L"删除文件")) return;
        st.resultText = L"删除完成 " + c.nt;
        AddLog(st, L"完成 删除文件 " + c.nt);
    }

    static void DoCopy(AppState& st) {
        Conv a = Win32ToNtPath(st.fileA), b = Win32ToNtPath(st.fileB);
        if (!a.ok) { AddLog(st, L"失败 源路径=" + a.err); return; }
        if (!b.ok) { AddLog(st, L"失败 目标路径=" + b.err); return; }
        KC_REQUEST r = MakeReq(IOCTL_KC_COPY);
        PutPath(r, false, a.nt);
        PutPath(r, true, b.nt);
        auto rep = g_dev.Call(IOCTL_KC_COPY, r);
        if (!Good(st, rep, L"复制文件")) return;
        kc_u64 n = 0;
        if (rep->Bytes >= sizeof(n)) std::memcpy(&n, rep->Payload, sizeof(n));
        st.resultText = L"复制完成 bytes=" + Dec(n);
        AddLog(st, L"完成 复制文件 bytes=" + Dec(n));
    }

    static void DoRmtree(AppState& st) {
        Conv c = Win32ToNtPath(st.rmtreePath);
        if (!c.ok) { AddLog(st, L"失败 路径=" + c.err); return; }
        KC_REQUEST r = MakeReq(IOCTL_KC_RD);
        PutPath(r, false, c.nt);
        auto rep = g_dev.Call(IOCTL_KC_RD, r);
        if (!Good(st, rep, L"递归删除")) return;
        KC_RD_RESULT x = ReadPayload<KC_RD_RESULT>(*rep);
        std::wstring s = L"files=" + std::to_wstring(x.DeletedFiles) +
            L" dirs=" + std::to_wstring(x.DeletedDirs) +
            L" failed=" + std::to_wstring(x.Failed) +
            L" partial=" + std::to_wstring(x.Partial);
        st.resultText = s;
        AddLog(st, L"完成 递归删除 " + s);
    }

    static void DoFProtect(AppState& st, kc_u32 op, const wchar_t* name) {
        KC_REQUEST r = MakeReq(IOCTL_KC_FPROTECT, 0, op);
        if (op == KC_FP_ADD || op == KC_FP_REMOVE) {
            Conv c = Win32ToNtPath(st.fpPath);
            if (!c.ok) { AddLog(st, L"失败 路径=" + c.err); return; }
            PutPath(r, false, c.nt);
        }
        auto rep = g_dev.Call(IOCTL_KC_FPROTECT, r);
        if (!Good(st, rep, name)) return;
        KC_FP_RESULT f = ReadPayload<KC_FP_RESULT>(*rep);
        st.fpRows.clear();
        for (kc_u32 i = 0; i < (kc_u32)KC_FP_MAX; ++i) {
            std::wstring s = Unpack(f.Rules[i], KC_MAX_PATH);
            if (!s.empty()) st.fpRows.push_back({ std::to_wstring(i), s });
        }
        std::wstring line = L"rules=" + std::to_wstring(f.Count) +
            L" blocked=" + std::to_wstring(f.Blocked) +
            L" observed=" + std::to_wstring(f.Observed);
        st.resultText = line;
        AddLog(st, L"完成 " + std::wstring(name) + L" " + line);
    }

    static void DoRegQuery(AppState& st) {
        Conv c = RegPathToNtPath(st.regPath, st.interactiveSid);
        if (!c.ok) { AddLog(st, L"失败 注册表路径=" + c.err); return; }
        if (st.regValue.empty()) { AddLog(st, L"失败 值名不能为空"); return; }
        kc_u32 flags = st.wow32 ? (kc_u32)KC_FLAG_WOW32 : 0u;
        KC_REQUEST r = MakeReq(IOCTL_KC_REGQUERY, 0, flags);
        PutPath(r, false, c.nt);
        PutPath(r, true, st.regValue);
        auto rep = g_dev.Call(IOCTL_KC_REGQUERY, r);
        if (!Good(st, rep, L"注册表查询")) return;
        KC_REG_RESULT x = ReadPayload<KC_REG_RESULT>(*rep);
        std::wstring val;
        if (x.ValueType == REG_SZ || x.ValueType == REG_EXPAND_SZ) {
            for (kc_u32 i = 0; i + 1 < x.DataLength; i += 2) {
                wchar_t ch = (wchar_t)(x.Data[i] | (x.Data[i + 1] << 8));
                if (!ch) break;
                val.push_back(ch);
            }
        }
        else if (x.ValueType == REG_DWORD && x.DataLength >= 4) {
            unsigned v = x.Data[0] | (x.Data[1] << 8) | (x.Data[2] << 16) |
                ((unsigned)x.Data[3] << 24);
            val = std::to_wstring(v);
        }
        else {
            wchar_t b[8];
            val = L"hex:";
            for (kc_u32 i = 0; i < x.DataLength && i < 32; ++i) {
                std::swprintf(b, 8, L"%02X", (unsigned)x.Data[i]);
                val += b;
            }
        }
        st.regRows.clear();
        st.regRows.push_back({ L"NT 路径", c.nt });
        st.regRows.push_back({ L"值名", st.regValue });
        st.regRows.push_back({ L"类型", std::to_wstring(x.ValueType) });
        st.regRows.push_back({ L"长度", std::to_wstring(x.DataLength) });
        st.regRows.push_back({ L"值", val });
        st.resultText = L"type=" + std::to_wstring(x.ValueType) +
            L" len=" + std::to_wstring(x.DataLength) + L" val=" + val;
        AddLog(st, L"完成 注册表查询 " + st.resultText);
    }

    static void DoEvents(AppState& st, bool logOk) {
        kc_u32 code = st.eventQueue == 0
            ? (kc_u32)IOCTL_KC_CALLBACK_POLL : (kc_u32)IOCTL_KC_EVENT_PULL;
        auto rep = g_dev.Call(code, MakeReq(code));
        if (!Good(st, rep, st.eventQueue == 0 ? L"读取共享队列" : L"读取独立队列")) return;
        KC_EVENT_BATCH b = ReadPayload<KC_EVENT_BATCH>(*rep);
        kc_u32 n = (std::min)(b.Count, EventCap(*rep));
        for (kc_u32 i = 0; i < n; ++i) {
            const KC_EVENT& e = b.Events[i];
            std::vector<std::wstring> row = {
                EvtName(e.Type), std::to_wstring(e.ProcessId), Hex(e.Operation),
                Hex(e.Status), Dec(e.TimeStamp), Unpack(e.Detail, KC_DETAIL)
            };
            st.eventRows.push_back(row);
        }
        if (st.eventRows.size() > 400)
            st.eventRows.erase(st.eventRows.begin(),
                st.eventRows.begin() + (st.eventRows.size() - 400));
        std::wstring line = L"count=" + std::to_wstring(n) +
            L" dropped=" + std::to_wstring(b.Reserved);
        st.resultText = line;
        if (logOk) AddLog(st, L"完成 读取事件队列 " + line);
    }

    static void ShowEnumPage(AppState& st, const KC_ENUM_PAGE& p, kc_u32 cap) {
        kc_u32 n = (std::min)(p.Count, cap);
        st.enumSession = p.Session;
        st.enumTotal = p.Total;
        st.enumMore = p.More;
        st.enumCursor += p.Count;
        for (kc_u32 i = 0; i < n; ++i) {
            const KC_ENUM_ITEM& it = p.Items[i];
            st.enumRows.push_back({
                std::to_wstring(it.Id), Hex(it.Flags), Dec(it.Size),
                Unpack(it.Name, KC_ENUM_NAME)
                });
        }
        st.resultText = L"session=" + std::to_wstring(p.Session) +
            L" total=" + std::to_wstring(p.Total) +
            L" count=" + std::to_wstring(n) +
            L" more=" + std::to_wstring(p.More) +
            L" cursor=" + std::to_wstring(st.enumCursor);
        AddLog(st, L"完成 枚举分页 " + st.resultText);
    }

    static void DoEnumOpen(AppState& st, kc_u32 kind) {
        KC_REQUEST r = MakeReq(IOCTL_KC_ENUM_OPEN, kind,
            st.strictEnum ? (kc_u32)KC_ENUM_STRICT_LIVENESS : 0u);
        if (kind == KC_ENUM_KIND_DIR) {
            Conv c = Win32ToNtPath(st.enumDir);
            if (!c.ok) { AddLog(st, L"失败 路径=" + c.err); return; }
            PutPath(r, false, c.nt);
        }
        auto rep = g_dev.Call(IOCTL_KC_ENUM_OPEN, r);
        if (!Good(st, rep, L"打开枚举会话")) return;
        KC_ENUM_OPEN_RESULT o = ReadPayload<KC_ENUM_OPEN_RESULT>(*rep);
        st.enumSession = o.Session;
        st.enumCursor = 0;
        st.enumTotal = o.Total;
        st.enumRows.clear();
        st.sessionInput = std::to_wstring(o.Session);
        st.resultText = L"session=" + std::to_wstring(o.Session) +
            L" total=" + std::to_wstring(o.Total);
        AddLog(st, L"完成 打开枚举会话 " + st.resultText);
    }

    static void DoEnumNext(AppState& st) {
        auto s = ParseNum(st.sessionInput);
        if (!s) { AddLog(st, L"失败 枚举会话句柄无效"); return; }
        auto rep = g_dev.Call(IOCTL_KC_ENUM_NEXT,
            MakeReq(IOCTL_KC_ENUM_NEXT, *s, st.enumCursor));
        if (!Good(st, rep, L"枚举下一页")) return;
        ShowEnumPage(st, ReadPayload<KC_ENUM_PAGE>(*rep), PageCap(*rep));
    }

    static void DoEnumAuto(AppState& st, kc_u32 kind) {
        DoEnumOpen(st, kind);
        if (st.enumSession == 0) return;
        st.enumRows.clear();
        st.enumCursor = 0;
        for (;;) {
            auto rep = g_dev.Call(IOCTL_KC_ENUM_NEXT,
                MakeReq(IOCTL_KC_ENUM_NEXT, st.enumSession, st.enumCursor));
            if (!rep || rep->Status != KC_OK) {
                AddLog(st, L"失败 枚举自动分页 " +
                    (rep ? Hex(rep->Status) + L" " + StatusName(rep->Status) : L"设备调用失败"));
                break;
            }
            KC_ENUM_PAGE p = ReadPayload<KC_ENUM_PAGE>(*rep);
            kc_u32 n = (std::min)(p.Count, PageCap(*rep));
            for (kc_u32 i = 0; i < n; ++i) {
                const KC_ENUM_ITEM& it = p.Items[i];
                st.enumRows.push_back({
                    std::to_wstring(it.Id), Hex(it.Flags), Dec(it.Size),
                    Unpack(it.Name, KC_ENUM_NAME)
                    });
            }
            st.enumCursor += p.Count;
            if (!p.More || p.Count == 0 || st.enumRows.size() > 8000) break;
        }
        st.resultText = L"total=" + std::to_wstring(st.enumTotal) +
            L" shown=" + std::to_wstring(st.enumRows.size());
        (void)g_dev.Call(IOCTL_KC_ENUM_CLOSE, MakeReq(IOCTL_KC_ENUM_CLOSE, st.enumSession));
        AddLog(st, L"完成 自动枚举 " + st.resultText);
    }

    static void DoEnumClose(AppState& st) {
        auto s = ParseNum(st.sessionInput);
        if (!s) { AddLog(st, L"失败 枚举会话句柄无效"); return; }
        auto rep = g_dev.Call(IOCTL_KC_ENUM_CLOSE, MakeReq(IOCTL_KC_ENUM_CLOSE, *s));
        if (!Good(st, rep, L"关闭枚举会话")) return;
        st.enumSession = 0;
        st.enumCursor = 0;
        AddLog(st, L"完成 关闭枚举会话 session=" + std::to_wstring(*s));
    }

    static void DoStatus(AppState& st, bool logOk) {
        auto rep = g_dev.Call(IOCTL_KC_STATUS, MakeReq(IOCTL_KC_STATUS));
        if (!Good(st, rep, L"读取状态")) return;
        st.status = ReadPayload<KC_STATUS_RESULT>(*rep);
        st.statusRows.clear();
        st.statusRows.push_back({ L"cap", Hex(st.status.Capabilities) });
        st.statusRows.push_back({ L"lifecycle", std::to_wstring(st.status.Lifecycle) });
        st.statusRows.push_back({ L"active", std::to_wstring(st.status.ActiveRequests) });
        st.statusRows.push_back({ L"build", std::to_wstring(st.status.BuildNumber) });
        st.statusRows.push_back({ L"legacydrop", std::to_wstring(st.status.LegacyDropped) });
        st.statusRows.push_back({ L"fanout", std::to_wstring(st.status.FanoutSlots) });
        st.statusRows.push_back({ L"enumopen", std::to_wstring(st.status.EnumOpen) });
        st.statusRows.push_back({ L"cbmask", Hex(st.status.CallbackMask) });
        st.statusRows.push_back({ L"flt", std::to_wstring(st.status.FltState) });
        st.cbProc = (st.status.CallbackMask & KC_CB_PROC) != 0;
        st.cbReg = (st.status.CallbackMask & KC_CB_REG) != 0;
        st.cbHandle = (st.status.CallbackMask & KC_CB_HANDLE) != 0;
        st.cbFile = (st.status.CallbackMask & KC_CB_FILE) != 0;
        st.statusText = L"flt=" + std::to_wstring(st.status.FltState) +
            L" active=" + std::to_wstring(st.status.ActiveRequests);
        if (logOk) AddLog(st, L"完成 读取状态 " + st.statusText);
    }

    static const wchar_t* kIrpNames[KC_FLT_OPS] = {
        L"CREATE", L"CREATE_NAMED_PIPE", L"CLOSE", L"READ", L"WRITE",
        L"QUERY_INFORMATION", L"SET_INFORMATION", L"QUERY_EA", L"SET_EA", L"FLUSH_BUFFERS",
        L"QUERY_VOLUME_INFORMATION", L"SET_VOLUME_INFORMATION", L"DIRECTORY_CONTROL",
        L"FILE_SYSTEM_CONTROL", L"DEVICE_CONTROL", L"INTERNAL_DEVICE_CONTROL",
        L"SHUTDOWN", L"LOCK_CONTROL", L"CLEANUP", L"CREATE_MAILSLOT",
        L"QUERY_SECURITY", L"SET_SECURITY", L"POWER", L"SYSTEM_CONTROL",
        L"DEVICE_CHANGE", L"QUERY_QUOTA", L"SET_QUOTA", L"PNP",
        L"PNP_POWER", L"MJ_29", L"MJ_30", L"MJ_31"
    };

    static void DoFileStat(AppState& st, bool logOk) {
        auto rep = g_dev.Call(IOCTL_KC_FILE_STAT, MakeReq(IOCTL_KC_FILE_STAT));
        if (!Good(st, rep, L"读取文件 I/O 统计")) return;
        KC_FILE_STAT x = ReadPayload<KC_FILE_STAT>(*rep);
        st.fileStatRows.clear();
        st.fileStatRows.push_back({ L"filter", std::to_wstring(x.FilterRegistered) });
        st.fileStatRows.push_back({ L"started", std::to_wstring(x.FilteringStarted) });
        st.fileStatRows.push_back({ L"instances", std::to_wstring(x.InstanceCount) });
        st.fileStatRows.push_back({ L"observing", std::to_wstring(x.Observing) });
        st.fileStatRows.push_back({ L"attach", Hex(x.AttachStatus) });
        st.fileStatRows.push_back({ L"rules", std::to_wstring(x.RuleCount) });
        st.fileStatRows.push_back({ L"blocked", std::to_wstring(x.Blocked) });
        for (int i = 0; i < (int)KC_FLT_OPS; ++i)
            if (x.Ops[i]) st.fileStatRows.push_back({ kIrpNames[i], std::to_wstring(x.Ops[i]) });
        if (logOk)
            AddLog(st, L"完成 读取文件 I/O 统计 instances=" + std::to_wstring(x.InstanceCount));
    }

    static void DoCallback(AppState& st, bool on) {
        kc_u32 mask = 0;
        if (on) {
            if (st.cbProc)   mask |= KC_CB_PROC;
            if (st.cbReg)    mask |= KC_CB_REG;
            if (st.cbHandle) mask |= KC_CB_HANDLE;
            if (st.cbFile)   mask |= KC_CB_FILE;
        }
        else mask = KC_CB_PROC | KC_CB_REG | KC_CB_HANDLE | KC_CB_FILE;
        kc_u32 code = on ? (kc_u32)IOCTL_KC_CALLBACK_ON : (kc_u32)IOCTL_KC_CALLBACK_OFF;
        auto rep = g_dev.Call(code, MakeReq(code, mask));
        if (!Good(st, rep, on ? L"开启回调" : L"关闭回调")) return;
        AddLog(st, on ? L"完成 开启回调 mask=" + Hex(mask)
            : L"完成 停用全部回调");
        DoStatus(st, false);
    }

    static void ExecutePending(AppState& st) {
        switch (st.pending) {
        case Pending::Del:             DoDel(st); break;
        case Pending::Rmtree:          DoRmtree(st); break;
        case Pending::Kill:            DoKill(st); break;
        case Pending::Strip:           DoStrip(st); break;
        case Pending::CriticalToggle:  DoCriticalToggle(st); break;
        default: break;
        }
        st.pending = Pending::None;
    }

    static void DrawLog(example::ExampleGUI& g, AppState& st, float rows) {
        g.ExampleSeparator();
        g.ExampleLog(L"最近操作", st.logs, rows * example::kRowH + 4.f);
    }

    static void DrawPage1(example::ExampleGUI& g, AppState& st) {
        g.ExampleInput(L"源路径 A", st.fileA);
        g.ExampleInput(L"目标路径 B", st.fileB);
        g.ExampleInput(L"递归删除目录", st.rmtreePath);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"删除文件", false, 130.f)) BeginPending(st, Pending::Del, L"删除文件");
        if (g.ExampleButton(L"复制文件", false, 130.f)) DoCopy(st);
        if (g.ExampleButton(L"递归删除", false, 130.f)) BeginPending(st, Pending::Rmtree, L"递归删除");
        g.ExampleEndHorizontal();
        g.ExampleLabel(st.resultText.c_str(), true);
        g.ExampleSeparator();
        g.ExampleInput(L"删除保护规则路径", st.fpPath);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"添加规则", false, 110.f)) DoFProtect(st, KC_FP_ADD, L"添加规则");
        if (g.ExampleButton(L"移除规则", false, 110.f)) DoFProtect(st, KC_FP_REMOVE, L"移除规则");
        if (g.ExampleButton(L"清空规则", false, 110.f)) DoFProtect(st, KC_FP_CLEAR, L"清空规则");
        if (g.ExampleButton(L"列出规则", false, 110.f)) DoFProtect(st, KC_FP_LIST, L"列出规则");
        g.ExampleEndHorizontal();
        g.ExampleColumnHeader({ L"序号", L"规则路径" });
        g.ExampleTable(nullptr, st.fpHeaders, st.fpRows,
            (float)(std::max)((size_t)3, st.fpRows.size()) * example::kRowH + 4.f,
            false, nullptr, 3002);
        g.ExampleSpace(8.f);
    }

    static void DrawPage2(example::ExampleGUI& g, AppState& st) {
        g.ExampleInput(L"注册表路径", st.regPath);
        g.ExampleInput(L"值名", st.regValue);
        g.ExampleCheckbox(L"32 位注册表视图", st.wow32);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"查询", true, 130.f)) DoRegQuery(st);
        g.ExampleEndHorizontal();
        g.ExampleLabel(st.resultText.c_str(), true);
        g.ExampleSpace(4.f);
        g.ExampleColumnHeader({ L"字段", L"值" });
        g.ExampleTable(nullptr, st.regHeaders, st.regRows,
            (float)(std::max)((size_t)5, st.regRows.size()) * example::kRowH + 4.f,
            false, nullptr, 3003);
        g.ExampleSpace(8.f);
    }

    static void DrawPage3(example::ExampleGUI& g, AppState& st) {
        g.ExampleDropdown(L"事件队列", st.eventQueue, { L"共享队列 legacy", L"独立队列 fanout" });
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"读取事件队列", true, 130.f)) DoEvents(st, true);
        if (g.ExampleButton(L"清空显示", false, 130.f)) st.eventRows.clear();
        g.ExampleCheckbox(L"自动读取独立队列", st.autoPull);
        g.ExampleEndHorizontal();
        g.ExampleLabel(st.resultText.c_str(), true);
        g.ExampleSpace(4.f);
        g.ExampleColumnHeader({ L"类型", L"PID", L"操作", L"状态", L"时间", L"详情" });
        g.ExampleTable(nullptr, st.eventHeaders, st.eventRows,
            (float)(std::max)((size_t)8, st.eventRows.size()) * example::kRowH + 4.f,
            false, &st.ctxRow, 3004);
        DrawLog(g, st, 8.f);
        g.ExampleSpace(8.f);
    }

    static void DrawPage4(example::ExampleGUI& g, AppState& st) {
        g.ExampleInput(L"目录路径", st.enumDir);
        g.ExampleInput(L"枚举会话句柄", st.sessionInput);
        g.ExampleDropdown(L"会话类型", st.enumKind, { L"进程 proc", L"目录 dir" });
        g.ExampleCheckbox(L"严格存活校验", st.strictEnum);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"枚举进程", true, 120.f)) DoEnumAuto(st, KC_ENUM_KIND_PROC);
        if (g.ExampleButton(L"枚举目录", true, 120.f)) DoEnumAuto(st, KC_ENUM_KIND_DIR);
        g.ExampleEndHorizontal();
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"打开会话", false, 120.f))
            DoEnumOpen(st, st.enumKind == 0 ? (kc_u32)KC_ENUM_KIND_PROC : (kc_u32)KC_ENUM_KIND_DIR);
        if (g.ExampleButton(L"下一页", false, 120.f)) DoEnumNext(st);
        if (g.ExampleButton(L"关闭会话", false, 120.f)) DoEnumClose(st);
        g.ExampleEndHorizontal();
        g.ExampleLabel(st.resultText.c_str(), true);
        g.ExampleSpace(4.f);
        g.ExampleColumnHeader({ L"ID", L"标志", L"大小", L"名称" });
        g.ExampleTable(nullptr, st.enumHeaders, st.enumRows,
            (float)(std::max)((size_t)8, st.enumRows.size()) * example::kRowH + 4.f,
            false, &st.ctxRow, 3005);
        g.ExampleSpace(8.f);
    }

    static void DrawPage5(example::ExampleGUI& g, AppState& st) {
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"立即重新枚举", true, 130.f)) DoStatus(st, true);
        if (g.ExampleButton(L"文件 I/O 统计", false, 130.f)) DoFileStat(st, true);
        g.ExampleEndHorizontal();
        g.ExampleLabel(st.statusText.c_str(), true);
        g.ExampleSpace(4.f);
        g.ExampleColumnHeader({ L"字段", L"值" });
        g.ExampleTable(nullptr, st.statusHeaders, st.statusRows,
            (float)(std::max)((size_t)9, st.statusRows.size()) * example::kRowH + 4.f,
            false, nullptr, 3006);
        g.ExampleSeparator();
        g.ExampleColumnHeader({ L"项目", L"值" });
        g.ExampleTable(nullptr, st.fileStatHeaders, st.fileStatRows,
            (float)(std::max)((size_t)7, st.fileStatRows.size()) * example::kRowH + 4.f,
            false, nullptr, 3007);
        g.ExampleSeparator();
        g.ExampleLabel(L"回调启用状态", true);
        g.ExampleCheckbox(L"进程回调 proc", st.cbProc);
        g.ExampleCheckbox(L"注册表回调 reg", st.cbReg);
        g.ExampleCheckbox(L"句柄回调 handle", st.cbHandle);
        g.ExampleCheckbox(L"文件观察 file", st.cbFile);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"启用所选", false, 130.f)) DoCallback(st, true);
        if (g.ExampleButton(L"全部停用", false, 130.f)) DoCallback(st, false);
        g.ExampleEndHorizontal();
        DrawLog(g, st, 10.f);
        g.ExampleSpace(8.f);
    }

    static void DrawPage6(example::ExampleGUI& g, AppState& st) {
        g.ExampleLabel(L"分区遮罩（可捕获分区图，静态区跳过绘制）", true);
        if (g.ExampleToggle(L"启用分区遮罩", st.maskOverlay)) {
            g.ExampleSetMaskOverlay(st.maskOverlay);
            st.maskOverlayApplied = st.maskOverlay;
            AddLog(st, st.maskOverlay ? L"信息 分区遮罩已启用" : L"信息 分区遮罩已停用");
        }
        if (g.ExampleToggle(L"动态区遮罩（全界面分块）", st.masksEnabled)) {
            g.ExampleSetMasksEnabled(st.masksEnabled);
            st.masksEnabledApplied = st.masksEnabled;
            AddLog(st, st.masksEnabled ? L"信息 动态区遮罩已启用" : L"信息 动态区遮罩已停用");
        }
        if (g.ExampleButton(L"导出遮罩表 JSON", false, 190.f)) {
            std::wstring text, path;
            g.ExampleExportMask(text);
            if (ExportMaskJson(text, path))
                AddLog(st, L"完成 遮罩表已导出 " + path);
            else
                AddLog(st, L"失败 遮罩表导出失败");
        }
        g.ExampleMaskStats(st.maskTotal, st.maskSkipped, st.maskRedrawn);
        g.ExampleLabel((L"本帧分区数=" + std::to_wstring(st.maskTotal) +
            L"  跳过绘制=" + std::to_wstring(st.maskSkipped) +
            L"  执行绘制=" + std::to_wstring(st.maskRedrawn)).c_str(), true);
        g.ExampleSeparator();

        g.ExampleLabel(L"操作确认对话框", true);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"顶级 Z 序", st.confirmZ == 0, 110.f)) st.confirmZ = 0;
        if (g.ExampleButton(L"从属主窗口", st.confirmZ == 1, 130.f)) st.confirmZ = 1;
        if (g.ExampleButton(L"常规 Z 序", st.confirmZ == 2, 90.f)) st.confirmZ = 2;
        g.ExampleEndHorizontal();
        g.ExampleSeparator();

        g.ExampleLabel(L"Z 序与 UIAccess", true);
        if (g.ExampleButton(L"重新申请 UIAccess", false, 180.f)) {
            std::wstring ownSid = st.interactiveSid;
            if (ownSid.empty()) { auto sid = CurrentUserSid(); if (sid) ownSid = *sid; }
            if (Boot::RelaunchWithWinlogonToken(ownSid)) {
                PostQuitMessage(0);
            }
            else {
                st.bootText = L"标准启动（ZBID_DESKTOP）  " + Boot::LastBootError();
                AddLog(st, L"失败 UIAccess 申请未成功 " + Boot::LastBootError());
            }
        }
        if (g.ExampleToggle(L"总在最前 (WS_EX_TOPMOST)", st.zTopmost)) {
            g.SetAlwaysOnTop(st.zTopmost);
            st.zApplied = st.zTopmost;
            AddLog(st, st.zTopmost ? L"信息 已启用 WS_EX_TOPMOST" : L"信息 已停用 WS_EX_TOPMOST");
        }
        if (g.ExampleToggle(L"周期性重申 Z 序", st.zGuard)) {
            if (st.zGuard) g.EnableZOrderGuard((UINT)st.zGuardMs);
            else g.DisableZOrderGuard();
            st.zGuardApplied = st.zGuard;
            st.zGuardMsApplied = st.zGuardMs;
        }
        if (g.ExampleSlider(L"重申周期 (ms)", st.zGuardMs, 200.f, 3000.f) && st.zGuard) {
            g.EnableZOrderGuard((UINT)st.zGuardMs);
            st.zGuardMsApplied = st.zGuardMs;
        }
        std::wstring above = g.WindowAboveTitle();
        std::wstring clsName = g.WindowAboveClass();
        std::wstring band = st.uiAccess ? L"ZBID_UIACCESS" : L"ZBID_DESKTOP";
        g.ExampleLabel((L"当前 Z 序段: " + band + L"   UIAccess: " +
            std::wstring(st.uiAccess ? L"是" : L"否")).c_str(), true);
        g.ExampleLabel(st.identityText.c_str(), true);
        g.ExampleLabel((L"进程启动模式: " + st.bootText).c_str(), true);
        g.ExampleLabel((L"Z 序前序窗口: " +
            (above.empty() ? std::wstring(L"无") : above) + L"   窗口类: " +
            (clsName.empty() ? std::wstring(L"-") : clsName)).c_str(), true);
        g.ExampleSeparator();

        g.ExampleLabel(L"自动重新枚举", true);
        g.ExampleToggle(L"启用自动重新枚举", st.autoRefresh);
        bool sliderChanged = g.ExampleSlider(L"枚举间隔 (ms)", st.refreshMs, 100.f, 5000.f);
        if (sliderChanged) {
            wchar_t b[32];
            std::swprintf(b, 32, L"%d", (int)st.refreshMs);
            st.refreshMsText = b;
        }
        if (g.ExampleInput(L"间隔毫秒值", st.refreshMsText)) {
            auto v = ParseNum(st.refreshMsText);
            if (v && *v >= 100 && *v <= 5000) st.refreshMs = (float)*v;
        }
        g.ExampleSeparator();

        g.ExampleLabel(L"主题预设", true);
        g.ExampleBeginHorizontal();
        for (int i = 0; i < 4; ++i)
            if (g.ExampleButton(kThemeNames[i], st.theme == i, 130.f)) {
                ApplyTheme(i, st);
                st.lastAction = kThemeNames[i];
            }
        g.ExampleEndHorizontal();
        g.ExampleSpace(4.f);
        g.ExampleStylePreview();
        g.ExampleSeparator();

        g.ExampleLabel(L"主色 · 强调色", true);
        g.ExampleBeginHorizontal();
        for (int i = 0; i < 6; ++i)
            if (g.ExampleColorSwatch(kAccents[i].label, kAccents[i].rgb,
                ComposeAccent(st) == kAccents[i].rgb, 104.f)) {
                example::ExampleSetAccent(kAccents[i].rgb);
                SyncAccent(st, kAccents[i].rgb);
                st.lastAction = kAccents[i].label;
            }
        g.ExampleEndHorizontal();
        {
            bool ch = false;
            ch |= g.ExampleSlider(L"强调色 红", st.accentR, 0.f, 1.f);
            ch |= g.ExampleSlider(L"强调色 绿", st.accentG, 0.f, 1.f);
            ch |= g.ExampleSlider(L"强调色 蓝", st.accentB, 0.f, 1.f);
            if (ch) {
                example::ExampleSetAccent(ComposeAccent(st));
                st.lastAction = L"自定义强调色";
            }
        }
        g.ExampleSeparator();

        g.ExampleLabel(L"背景颜色", true);
        g.ExampleBeginHorizontal();
        for (int i = 0; i < 6; ++i)
            if (g.ExampleColorSwatch(kBackgrounds[i].label, kBackgrounds[i].rgb,
                ComposeBackground(st) == kBackgrounds[i].rgb, 104.f)) {
                example::ExampleSetBackground(kBackgrounds[i].rgb);
                SyncBackground(st, kBackgrounds[i].rgb);
                st.lastAction = kBackgrounds[i].label;
            }
        g.ExampleEndHorizontal();
        {
            bool ch = false;
            ch |= g.ExampleSlider(L"背景基色 红", st.bgR, 0.f, 1.f);
            ch |= g.ExampleSlider(L"背景基色 绿", st.bgG, 0.f, 1.f);
            ch |= g.ExampleSlider(L"背景基色 蓝", st.bgB, 0.f, 1.f);
            if (ch) {
                example::ExampleSetBackground(ComposeBackground(st));
                st.lastAction = L"自定义背景色";
            }
        }
        g.ExampleSeparator();

        g.ExampleInput(L"设备对象路径", st.devicePath);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"应用并连接", true, 150.f)) {
            g_dev.SetPath(st.devicePath);
            if (g_dev.Open()) AddLog(st, L"完成 设备对象已连接");
            else AddLog(st, L"失败 打开设备对象 win32=" + std::to_wstring(g_dev.LastWin32()));
        }
        g.ExampleEndHorizontal();
        g.ExampleSeparator();

        g.ExampleLabel(L"布局", true);
        g.ExampleSlider(L"控件间距", example::kGap, 2.f, 24.f);
        g.ExampleSlider(L"导航项高度", example::kNavItemH, 24.f, 56.f);
        g.ExampleSlider(L"边框宽度", example::kBorderWidth, 0.5f, 5.f);
        g.ExampleSeparator();

        g.ExampleLabel(L"字体与动效", true);
        g.ExampleSlider(L"字体大小", example::kFontScale, 0.8f, 1.6f);
        g.ExampleSlider(L"滑动速度", example::kScrollEase, 5.f, 50.f);
        g.ExampleSlider(L"滚轮行数", example::kScrollWheelLines, 1.f, 8.f);
        g.ExampleSlider(L"动画持续时间", st.fadeSec, 0.05f, 0.60f);
        g.ExampleToggle(L"启用切换动画", st.animEnabled);
        g.ExampleSeparator();

        if (g.ExampleButton(L"恢复默认值", false, 170.f)) ResetAll(st);
        g.ExampleSpace(6.f);
    }

    static void DrawBottomPanel(example::ExampleGUI& g, AppState& st,
        float X, float Y, float W, float H) {
        g.ExampleBeginPanel(nullptr, X, Y, W, H);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"模块视图", st.bottomTab == 0, 120.f)) {
            st.bottomTab = 0; st.bottomOffset = 0; RefreshBottom(st);
        }
        if (g.ExampleButton(L"句柄视图", st.bottomTab == 1, 120.f)) {
            st.bottomTab = 1; st.bottomOffset = 0; RefreshBottom(st);
        }
        if (g.ExampleButton(L"内存视图", st.bottomTab == 2, 120.f)) {
            st.bottomTab = 2; st.bottomOffset = 0; RefreshBottom(st);
        }
        if (g.ExampleButton(L"重新枚举详情", false, 120.f)) RefreshBottom(st);
        if (g.ExampleButton(L"上一页", false, 90.f)) {
            st.bottomOffset = (std::max)(0, st.bottomOffset - 8);
            RefreshBottom(st);
        }
        if (g.ExampleButton(L"下一页", false, 90.f)) {
            st.bottomOffset += 8;
            RefreshBottom(st);
        }
        g.ExampleEndHorizontal();
        g.ExampleBeginClip(X + 8.f, Y + 54.f, W - 16.f, H - 62.f);
        g.ExampleTable(nullptr, st.bottomHeaders, st.bottomRows,
            H - 76.f,
            true, nullptr, 3008);
        g.ExampleEndClip();
        g.ExampleEndPanel();
    }

    static int DrawContext(example::ExampleGUI& g, AppState& st) {
        if (st.page == 0)
            return g.ExampleContextMenu({
                { 1, L"查询保护级别", L"Enter" },
                { 2, L"剥离/附加PPL", nullptr },
                { 3, L"终止进程", L"Delete" },
                { 4, L"查询 BreakOnTermination", nullptr },
                { 0, nullptr, nullptr },
                { 11, L"总在最前", nullptr, true, st.zTopmost, true },
                { 5, L"复制进程 ID", nullptr },
                { 6, L"展开 / 折叠全部子树", nullptr }
                });
        if (st.page == 3)
            return g.ExampleContextMenu({
                { 7, L"清空事件视图", nullptr },
                { 8, L"读取事件队列", L"F5" }
                });
        return g.ExampleContextMenu({
            { 9, L"重新枚举当前页", L"F5" }
            });
    }

    static void HandleContext(AppState& st, int cmd) {
        switch (cmd) {
        case 1: DoProtect(st, L"查询保护级别", true); break;
        case 2: BeginPending(st, Pending::Strip, L"剥离/附加PPL"); break;
        case 3: BeginPending(st, Pending::Kill, L"终止进程"); break;
        case 4: DoCritical(st, KC_CRIT_OP_QUERY, L"查询 BreakOnTermination", true); break;
        case 5:
            if (st.selectedPid) st.pidInput = std::to_wstring(st.selectedPid);
            break;
        case 11:
            st.zTopmost = !st.zTopmost;
            AddLog(st, st.zTopmost ? L"信息 已启用 WS_EX_TOPMOST" : L"信息 已停用 WS_EX_TOPMOST");
            break;
        case 6:
            if (st.collapsed.empty()) {
                for (const auto& kv : st.nodes) {
                    if (kv.second.hasChild) st.collapsed.insert(kv.first);
                }
            }
            else {
                st.collapsed.clear();
            }
            RebuildOrder(st);
            break;
        case 7: st.eventRows.clear(); break;
        case 8: DoEvents(st, true); break;
        case 9:
            if (st.page == 0)      RefreshProcs(st);
            else if (st.page == 5) DoStatus(st, true);
            else if (st.page == 3) DoEvents(st, true);
            break;
        default: break;
        }
    }

    static void AutoRefreshTick(AppState& st) {
        auto now = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - st.lastRefresh).count();
        if (ms < (long long)st.refreshMs) return;
        st.lastRefresh = now;
        SampleGlobal(st);
        if (st.autoRefresh) {
            if (st.page == 0) {
                RefreshProcs(st);
                RefreshBottom(st);
            }
            else if (st.page == 5) DoStatus(st, false);
        }
        if (st.autoPull && st.page == 3 && st.eventQueue == 1) DoEvents(st, false);
    }

}

static HMODULE LoadFontDataModule() {
    wchar_t path[1024]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, 1024);
    if (!n || n >= 1024) return nullptr;
    std::wstring full(path, n);
    const std::size_t k = full.find_last_of(L"\\/");
    if (k == std::wstring::npos) return nullptr;
    full.replace(k + 1, std::wstring::npos, L"KernCmdFonts.dll");
    return LoadLibraryExW(full.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE);
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ PWSTR lpCmd, _In_ int) {
    std::wstring cmd = lpCmd ? lpCmd : L"";
    if (!Boot::IsElevated()) {
        Boot::SelfElevate(L"");
        return 0;
    }
    std::wstring bootFailure;
    if (!HasUIAccess() && !Boot::HasFlag(cmd, L"--boot-uia")) {
        std::wstring ownSid;
        auto sid = CurrentUserSid();
        if (sid) ownSid = *sid;
        if (Boot::RelaunchWithWinlogonToken(ownSid)) return 0;
        bootFailure = Boot::LastBootError();
    }

    if (HMODULE fm = LoadFontDataModule())
        example::ExampleIconFonts::SetFontModule(fm);
    example::ExampleIconFonts::Ensure();
    AppState st;
    if (!example::ExampleIconFonts::Ready()) {
        st.pageIcons = st.pageIconsLegacy;
        st.pageIconsOn = st.pageIconsLegacy;
        AddLog(st, L"失败 Fluent 图标字体未加载，侧栏已回退");
    }
    st.uiAccess = HasUIAccess();
    st.interactiveSid = Boot::Arg(cmd, L"--sid=");
    if (st.interactiveSid.empty()) {
        auto sid = CurrentUserSid();
        if (sid) st.interactiveSid = *sid;
    }
    st.identityText = IdentityLine();
    st.tiGroup = st.identityText.find(L"TrustedInstaller 组 SID=是") != std::wstring::npos;
    st.bootText = Boot::HasFlag(cmd, L"--boot-uia")
        ? (st.uiAccess ? L"Winlogon 令牌中继启动"
            : L"UIAccess 未获 win32k 承认（仍位于 ZBID_DESKTOP）")
        : (st.uiAccess ? L"已具备 UIAccess" : L"标准启动（ZBID_DESKTOP）");
    if (!bootFailure.empty()) st.bootText += L"  " + bootFailure;
    example::ExampleGUI gui;
    if (!gui.Create(hInst, L"Kernel_Tools 安全分析工具", 1240, 860)) {
        MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_ICONERROR);
        return 1;
    }
    gui.EnableAutoRefresh(50);
    if (!g_confirm.Create(hInst, gui.Handle()))
        AddLog(st, L"失败 操作确认对话框创建失败 win32=" +
            std::to_wstring(GetLastError()));

    gui.OnDraw([&st](example::ExampleGUI& g) {
        example::kPageFadeSec = st.animEnabled ? st.fadeSec : 0.f;
        AutoRefreshTick(st);

        const int confirmResult = g_confirm.Poll();
        if (confirmResult == 1) ExecutePending(st);
        else if (confirmResult == 2) st.pending = Pending::None;
        if (st.pending != Pending::None && !g_confirm.Visible()) {
            if (g_confirm.Handle()) {
                g_confirm.Show(st.pendingText.c_str());
                AddLog(st, L"信息 操作确认对话框已显示");
            }
            else {
                AddLog(st, L"失败 操作确认对话框不可用，操作已取消");
                st.pending = Pending::None;
            }
        }
        if (st.confirmZ != st.confirmZApplied) {
            g_confirm.SetZMode(st.confirmZ);
            st.confirmZApplied = st.confirmZ;
        }
        example::ExampleConfirmWindow::BroadcastRedraw();

        const auto nowId = std::chrono::steady_clock::now();
        if (st.lastIdentity == std::chrono::steady_clock::time_point{} ||
            std::chrono::duration_cast<std::chrono::milliseconds>(
                nowId - st.lastIdentity).count() >= 1000) {
            st.lastIdentity = nowId;
            st.identityText = IdentityLine();
            st.tiGroup = st.identityText.find(L"TrustedInstaller 组 SID=是") != std::wstring::npos;
            st.uiAccess = HasUIAccess();
        }

        bool conn = g_dev.Connected();
        if (conn != st.lastConnected) {
            g.ExampleSetConnected(conn);
            st.lastConnected = conn;
        }
        if (st.zTopmost != st.zApplied) {
            g.SetAlwaysOnTop(st.zTopmost);
            st.zApplied = st.zTopmost;
            AddLog(st, st.zTopmost ? L"信息 已启用 WS_EX_TOPMOST" : L"信息 已停用 WS_EX_TOPMOST");
        }
        if (st.maskOverlay != st.maskOverlayApplied) {
            g.ExampleSetMaskOverlay(st.maskOverlay);
            st.maskOverlayApplied = st.maskOverlay;
        }
        if (st.masksEnabled != st.masksEnabledApplied) {
            g.ExampleSetMasksEnabled(st.masksEnabled);
            st.masksEnabledApplied = st.masksEnabled;
        }
        if (st.zGuard != st.zGuardApplied ||
            (st.zGuard && st.zGuardMs != st.zGuardMsApplied)) {
            if (st.zGuard) g.EnableZOrderGuard((UINT)st.zGuardMs);
            else g.DisableZOrderGuard();
            st.zGuardApplied = st.zGuard;
            st.zGuardMsApplied = st.zGuardMs;
        }

        const float W = g.ContentWidth();
        const float H = g.ContentHeight();
        const float top = kTitleH + kToolbarH;
        const float bodyH = H - top;
        const unsigned long long pc = g.ExamplePointerCell();
        const unsigned long long vis = Sig({
            (unsigned long long)st.theme,
            (unsigned long long)(unsigned)(st.accentR * 1000.f),
            (unsigned long long)(unsigned)(st.accentG * 1000.f),
            (unsigned long long)(unsigned)(st.accentB * 1000.f),
            g_dev.Connected() ? 1ull : 0ull });
        if (vis != st.lastVisSig) {
            g.ExampleInvalidateMasks();
            g.RequestRedraw();
            st.lastVisSig = vis;
        }

        g.ExampleBeginMask(1, L"标题标识", 0.f, 0.f, W - 430.f, kTitleH,
            true, Sig({ 0x5449544Cull, vis }));
        g.ExampleAbsRect(0.f, 0.f, W, kTitleH,
            example::color::Panel, example::color::PanelStroke);
        g.ExampleAbsRect(12.f, 11.f, 20.f, 20.f, 0xFF6A00u, 0xFF6A00u);
        g.ExampleTextAt(42.f, 5.f, 520.f, 32.f,
            L"Kernel_Tools 安全分析工具", example::color::Text, 2,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        g.ExampleEndMask();
        g.ExampleBeginMask(2, L"CPU 与内存", W - 430.f, 0.f, 430.f, kTitleH,
            true, Sig({ HashW(st.hudText), vis }));
        g.ExampleTextAt(W - 420.f, 8.f, 404.f, 26.f,
            st.hudText.c_str(), example::color::TextMuted, 3,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        g.ExampleEndMask();

        g.ExampleBeginMask(3, L"工具条", 0.f, kTitleH, W, kToolbarH, true,
            Sig({ pc, (unsigned long long)st.page, HashW(st.filter),
                HashW(st.devicePath), st.zTopmost ? 1ull : 0ull,
                st.allowCritical ? 1ull : 0ull, vis }));
        g.ExampleAbsRect(0.f, kTitleH, W, kToolbarH,
            example::color::Panel, example::color::PanelStroke);
        g.ExampleBeginAbs(0.f, kTitleH, W, kToolbarH, 5.f);
        g.ExampleBeginHorizontal();
        if (g.ExampleButton(L"结束进程", true, 110.f)) {
            if (st.selectedPid) st.pidInput = std::to_wstring(st.selectedPid);
            BeginPending(st, Pending::Kill, L"终止进程");
        }
        if (g.ExampleButton(L"剥离/附加PPL", false, 130.f)) {
            if (st.selectedPid) st.pidInput = std::to_wstring(st.selectedPid);
            BeginPending(st, Pending::Strip, L"剥离/附加PPL");
        }
        if (g.ExampleButton(L"剥离/添加关键进程", false, 170.f)) {
            if (st.selectedPid) st.pidInput = std::to_wstring(st.selectedPid);
            BeginPending(st, Pending::CriticalToggle, L"剥离/添加关键进程");
        }
        if (g.ExampleButton(L"总在最前", st.zTopmost, 100.f)) st.zTopmost = !st.zTopmost;
        g.ExampleSearchBox(L"按映像名 / PID 筛选", st.filter, 230.f);
        if (g.ExampleButton(L"连接设备", false, 90.f)) {
            g_dev.SetPath(st.devicePath);
            if (g_dev.Open()) AddLog(st, L"完成 设备对象已连接");
            else AddLog(st, L"失败 打开设备对象 win32=" + std::to_wstring(g_dev.LastWin32()));
        }
        if (g.ExampleButton(L"断开设备", false, 90.f)) {
            g_dev.Close();
            AddLog(st, L"信息 设备对象已断开");
        }
        g.ExampleEndHorizontal();
        g.ExampleEndAbs();
        g.ExampleEndMask();

        g.ExampleBeginMask(4, L"导航", 0.f, top, kNavW, bodyH, true,
            Sig({ pc, (unsigned long long)st.page, vis }));
        g.ExampleAbsRect(0.f, top, kNavW, bodyH,
            example::color::NavBg, example::color::PanelStroke);
        g.ExampleBeginAbs(0.f, top, kNavW, bodyH, 4.f);
        for (int i = 0; i < (int)st.pageNames.size(); ++i)
            g.ExampleSideBarItem(st.pageIcons[i], st.pageIconsOn[i],
                st.pageNames[i].c_str(), i, st.page);
        g.ExampleEndAbs();
        g.ExampleEndMask();

        const std::size_t pageIdx = (static_cast<unsigned>(st.page) < 7u)
            ? static_cast<std::size_t>(st.page) : 0u;
        const float pageH = (st.page == 0) ? (bodyH - kBottomH) : bodyH;
        g.ExampleBeginMask(5, L"页面主体", kNavW, top, W - kNavW, pageH, false, 0ull);
        g.ExampleBeginPage(kNavW, top, W - kNavW, pageH,
            &st.pageScroll[pageIdx], &st.pageMeasured[pageIdx]);

        if (st.page == 0) {
            const int sortHit = g.ExampleSortHeader(
                { L"映像名", L"PID", L"CPU", L"工作集", L"公司名", L"文件描述", L"映像路径" },
                st.sortCol, st.sortAsc);
            if (sortHit >= 0) {
                if (sortHit == st.sortCol) st.sortAsc = !st.sortAsc;
                else { st.sortCol = sortHit; st.sortAsc = true; }
                RebuildOrder(st);
                g.RequestRedraw();
            }
            kc_u32 togglePid = 0, selectPid = 0;
            int rowIndex = 0;
            int iconBudget = 6;
            if (st.icons.map.size() > 768) st.icons.Clear();
            for (const auto& e : st.order) {
                auto it = st.nodes.find(e.first);
                if (it == st.nodes.end()) continue;
                const ProcNode& n = it->second;
                const std::wstring& iconKey = n.path.empty() ? n.name : n.path;
                HICON rowIcon = nullptr;
                auto ci = st.icons.map.find(iconKey);
                if (ci != st.icons.map.end()) rowIcon = ci->second;
                else if (iconBudget > 0 && !st.icons.failed.count(iconKey)) {
                    --iconBudget;
                    HICON h = LoadProcIcon(n.path, n.name);
                    if (h) { st.icons.map[iconKey] = h; rowIcon = h; }
                    else st.icons.failed.insert(iconKey);
                }
                auto hit = g.ExampleTreeRow(
                    n.name.c_str(), std::to_wstring(n.pid).c_str(),
                    CpuText(n.cpu).c_str(), MemSize(n.mem).c_str(),
                    n.company.c_str(), n.desc.c_str(), n.path.c_str(),
                    e.second, n.hasChild, !st.collapsed.count(n.pid),
                    n.state, n.pid == st.selectedPid,
                    &st.ctxRow, rowIndex, 4001, rowIcon);
                if (hit.toggled) togglePid = n.pid;
                if (hit.selected) selectPid = n.pid;
                ++rowIndex;
            }
            if (togglePid) {
                if (st.collapsed.count(togglePid)) st.collapsed.erase(togglePid);
                else st.collapsed.insert(togglePid);
                RebuildOrder(st);
                g.RequestRedraw();
            }
            if (selectPid) {
                st.selectedPid = selectPid;
                st.pidInput = std::to_wstring(selectPid);
                RefreshBottom(st);
                AddLog(st, L"信息 已选定进程 ID=" + st.pidInput);
            }
            g.ExampleSpace(6.f);
        }
        else {
            switch (st.page) {
            case 1: DrawPage1(g, st); break;
            case 2: DrawPage2(g, st); break;
            case 3: DrawPage3(g, st); break;
            case 4: DrawPage4(g, st); break;
            case 5: DrawPage5(g, st); break;
            case 6: DrawPage6(g, st); break;
            default: break;
            }
        }
        g.ExampleEndPage();
        g.ExampleEndMask();

        if (st.page == 0) {
            g.ExampleBeginMask(6, L"底部详情", kNavW, H - kBottomH,
                W - kNavW, kBottomH, false, 0ull);
            DrawBottomPanel(g, st, kNavW, H - kBottomH, W - kNavW, kBottomH);
            g.ExampleEndMask();
        }

        if (st.ctxRow >= 0 && st.page == 0 && st.ctxRow < (int)st.order.size()) {
            st.selectedPid = st.order[st.ctxRow].first;
            st.pidInput = std::to_wstring(st.selectedPid);
            RefreshBottom(st);
            AddLog(st, L"信息 右键已选定进程 ID=" + st.pidInput);
        }
        st.ctxRow = -1;

        HandleContext(st, DrawContext(g, st));
        });

    return gui.Run();
}


