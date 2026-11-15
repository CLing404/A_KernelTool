#pragma once
#ifndef EXAMPLE_FLUENT_GUI_H
#define EXAMPLE_FLUENT_GUI_H

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

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace example {

    using U32 = std::uint32_t;

    struct ExampleIconFonts {
        inline static std::vector<unsigned char> reg_{};
        inline static std::vector<unsigned char> fil_{};
        inline static HANDLE regH_ = nullptr;
        inline static HANDLE filH_ = nullptr;
        inline static std::wstring uiFace_{};
        inline static HMODULE fontMod_ = nullptr;
        inline static bool done_ = false;
        inline static bool ready_ = false;

        static bool LoadRes(int id, std::vector<unsigned char>& out) {
            HMODULE mods[2] = { fontMod_, GetModuleHandleW(nullptr) };
            for (HMODULE m : mods) {
                if (!m) continue;
                HRSRC r = FindResourceW(m, MAKEINTRESOURCEW(id), RT_RCDATA);
                if (!r) continue;
                const DWORD sz = SizeofResource(m, r);
                HGLOBAL g = LoadResource(m, r);
                const void* p = g ? LockResource(g) : nullptr;
                if (!p || !sz || sz > (1u << 28)) continue;
                out.assign(static_cast<const unsigned char*>(p),
                    static_cast<const unsigned char*>(p) + sz);
                return true;
            }
            return false;
        }

        static bool LoadFile(const wchar_t* name, std::vector<unsigned char>& out) {
            wchar_t path[1024]{};
            const DWORD n = GetModuleFileNameW(nullptr, path, 1024);
            if (!n || n >= 1024) return false;
            std::wstring full(path, n);
            const std::size_t k = full.find_last_of(L"\\/");
            if (k == std::wstring::npos) return false;
            full.replace(k + 1, std::wstring::npos, name);
            HANDLE hf = CreateFileW(full.c_str(), GENERIC_READ, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf == INVALID_HANDLE_VALUE) return false;
            LARGE_INTEGER sz{};
            BOOL ok = GetFileSizeEx(hf, &sz);
            DWORD rd = 0;
            if (ok && sz.QuadPart > 0 && sz.QuadPart < (1ll << 28)) {
                out.resize(static_cast<std::size_t>(sz.QuadPart));
                ok = ReadFile(hf, out.data(), static_cast<DWORD>(out.size()), &rd, nullptr)
                    && rd == out.size();
            }
            else ok = FALSE;
            CloseHandle(hf);
            if (!ok) out.clear();
            return ok != FALSE;
        }

        static void Ensure() {
            if (done_) return;
            done_ = true;
            const bool regOk = LoadRes(2001, reg_) ||
                LoadFile(L"FluentSystemIcons-Regular.ttf", reg_);
            const bool filOk = LoadRes(2002, fil_) ||
                LoadFile(L"FluentSystemIcons-Filled.ttf", fil_);
            DWORD n = 0;
            if (regOk && filOk) {
                regH_ = AddFontMemResourceEx(reg_.data(),
                    static_cast<DWORD>(reg_.size()), nullptr, &n);
                filH_ = AddFontMemResourceEx(fil_.data(),
                    static_cast<DWORD>(fil_.size()), nullptr, &n);
                ready_ = regH_ != nullptr && filH_ != nullptr;
            }
        }

        static bool Ready() noexcept { return ready_; }
        static void SetFontModule(HMODULE m) noexcept { fontMod_ = m; }
        static void SetUiFace(const wchar_t* f) { uiFace_ = f ? f : L""; }
        static const wchar_t* RegularFace() noexcept { return L"FluentSystemIcons-Regular"; }
        static const wchar_t* FilledFace() noexcept { return L"FluentSystemIcons-Filled"; }
        static const wchar_t* UiFace() noexcept {
            return uiFace_.empty() ? L"Microsoft YaHei UI" : uiFace_.c_str();
        }

        static void Shutdown() noexcept {
            if (regH_) { RemoveFontMemResourceEx(regH_); regH_ = nullptr; }
            if (filH_) { RemoveFontMemResourceEx(filH_); filH_ = nullptr; }
            ready_ = false;
        }
    };

    namespace color {
        inline U32 BgTop = 0x191A1E;
        inline U32 BgBottom = 0x202124;
        inline U32 Panel = 0x2B2C30;
        inline U32 PanelStroke = 0x3A3B40;
        inline U32 Control = 0x323338;
        inline U32 ControlHover = 0x3D3E44;
        inline U32 ControlPress = 0x484950;
        inline U32 Stroke = 0x4A4B52;
        inline U32 Text = 0xF3F3F5;
        inline U32 TextMuted = 0x9C9DA5;
        inline U32 TextDim = 0x6E6F77;
        inline U32 Accent = 0x60CDFF;
        inline U32 AccentPress = 0x3FA8D8;
        inline U32 OnAccent = 0x0C1116;
        inline U32 Ok = 0x6CCB5F;
        inline U32 Warn = 0xFCE100;
        inline U32 Error = 0xFF6A6A;
        inline U32 RowAlt = 0x2E2F34;
        inline U32 Header = 0x26272B;
        inline U32 NavBg = 0x242529;
        inline U32 NavActive = 0x35363C;
        inline U32 NewGreen = 0x39FF14;
        inline U32 TerminatedRed = 0xFF4545;
    }

    inline float kPanelPad = 16.f;
    inline float kCtrlH = 32.f;
    inline float kGap = 8.f;
    inline float kPanelR = 8.f;
    inline float kCtrlR = 4.f;
    inline float kRowH = 26.f;
    inline float kProcIconSize = 21.f;
    inline float kCardW = 860.f;
    inline float kPagePad = 10.f;
    inline float kNavW = 148.f;
    inline float kNavItemH = 34.f;
    inline float kNavGap = 6.f;
    inline float kBorderWidth = 1.6f;
    inline float kFontScale = 1.f;
    inline float kScrollEase = 20.f;
    inline float kPageFadeSec = 0.18f;
    inline float kPageSlidePx = 22.f;
    inline float kScrollWheelLines = 3.f;
    inline float kScrollPageFraction = 0.8f;
    inline float kCardH = 46.f;
    inline float kCardR = 5.f;
    inline float kSliderInputW = 72.f;

    struct ExampleMenuEntry {
        int id = 0;
        const wchar_t* label = nullptr;
        const wchar_t* shortcut = nullptr;
        bool enabled = true;
        bool checked = false;
        bool checkable = false;
    };

    enum class ExampleProcessState {
        ExampleNew,
        ExampleLongRunning,
        ExampleTerminated
    };

    struct ExampleProcessInfo {
        std::wstring name;
        std::wstring pid;
        std::wstring cpu;
        std::wstring memory;
        std::wstring imagePath;
    };

    static constexpr U32 CR(U32 c) noexcept {
        return (U32)(((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF));
    }

    inline void ExampleResetStyle() noexcept {
        color::BgTop = 0x191A1E;
        color::BgBottom = 0x202124;
        color::Panel = 0x2B2C30;
        color::PanelStroke = 0x3A3B40;
        color::Control = 0x323338;
        color::ControlHover = 0x3D3E44;
        color::ControlPress = 0x484950;
        color::Stroke = 0x4A4B52;
        color::Text = 0xF3F3F5;
        color::TextMuted = 0x9C9DA5;
        color::TextDim = 0x6E6F77;
        color::Accent = 0x60CDFF;
        color::AccentPress = 0x3FA8D8;
        color::OnAccent = 0x0C1116;
        color::Ok = 0x6CCB5F;
        color::Warn = 0xFCE100;
        color::Error = 0xFF6A6A;
        color::RowAlt = 0x2E2F34;
        color::Header = 0x26272B;
        color::NavBg = 0x242529;
        color::NavActive = 0x35363C;
        color::NewGreen = 0x39FF14;
        color::TerminatedRed = 0xFF4545;
        kPanelPad = 16.f;  kCtrlH = 32.f;  kGap = 8.f;
        kPanelR = 8.f;     kCtrlR = 4.f;   kRowH = 26.f;
        kCardW = 860.f;    kPagePad = 10.f; kNavW = 148.f;
        kNavItemH = 34.f;  kNavGap = 6.f;  kBorderWidth = 1.6f;
        kFontScale = 1.f;  kScrollEase = 20.f; kPageFadeSec = 0.18f;
        kPageSlidePx = 22.f; kScrollWheelLines = 3.f;
        kScrollPageFraction = 0.8f; kCardH = 46.f; kCardR = 5.f;
        kSliderInputW = 72.f;
    }

    inline void ExampleSetAccent(U32 rgb) noexcept {
        color::Accent = rgb;
        const int r = (int)((rgb >> 16) & 0xFF);
        const int g = (int)((rgb >> 8) & 0xFF);
        const int b = (int)(rgb & 0xFF);
        color::AccentPress = (U32)((((int)(r * 0.78f) & 0xFF) << 16)
            | (((int)(g * 0.78f) & 0xFF) << 8)
            | ((int)(b * 0.78f) & 0xFF));
    }

    inline U32 ExampleScaleColor(U32 rgb, float f, int add) noexcept {
        auto ch = [f, add](int v) {
            return (int)std::clamp((float)v * f + (float)add, 0.f, 255.f);
            };
        return (U32)((ch((int)((rgb >> 16) & 0xFF)) << 16)
            | (ch((int)((rgb >> 8) & 0xFF)) << 8)
            | ch((int)(rgb & 0xFF)));
    }

    inline void ExampleSetBackground(U32 rgb) noexcept {
        const int r = (int)((rgb >> 16) & 0xFF);
        const int g = (int)((rgb >> 8) & 0xFF);
        const int b = (int)(rgb & 0xFF);
        const float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        const bool light = (lum >= 128.f);
        if (light) {
            color::BgTop = ExampleScaleColor(rgb, 0.96f, 0);
            color::BgBottom = ExampleScaleColor(rgb, 1.04f, 0);
            color::Panel = ExampleScaleColor(rgb, 1.10f, 0);
            color::NavBg = ExampleScaleColor(rgb, 0.94f, 0);
            color::NavActive = ExampleScaleColor(rgb, 0.86f, 0);
            color::Header = ExampleScaleColor(rgb, 0.92f, 0);
            color::RowAlt = ExampleScaleColor(rgb, 1.04f, 0);
            color::Control = ExampleScaleColor(rgb, 1.14f, 0);
            color::ControlHover = ExampleScaleColor(rgb, 1.02f, -12);
            color::ControlPress = ExampleScaleColor(rgb, 0.90f, -20);
            color::PanelStroke = ExampleScaleColor(rgb, 0.80f, -24);
            color::Stroke = ExampleScaleColor(rgb, 0.72f, -32);
            color::Text = 0x16171A;
            color::TextMuted = 0x4A4C55;
            color::TextDim = 0x777A85;
        }
        else {
            color::BgTop = ExampleScaleColor(rgb, 0.80f, 0);
            color::BgBottom = ExampleScaleColor(rgb, 1.10f, 10);
            color::Panel = ExampleScaleColor(rgb, 1.18f, 14);
            color::NavBg = ExampleScaleColor(rgb, 0.92f, 6);
            color::NavActive = ExampleScaleColor(rgb, 1.35f, 24);
            color::Header = ExampleScaleColor(rgb, 1.05f, 10);
            color::RowAlt = ExampleScaleColor(rgb, 1.22f, 16);
            color::Control = ExampleScaleColor(rgb, 1.28f, 20);
            color::ControlHover = ExampleScaleColor(rgb, 1.45f, 26);
            color::ControlPress = ExampleScaleColor(rgb, 1.60f, 32);
            color::PanelStroke = ExampleScaleColor(rgb, 1.75f, 40);
            color::Stroke = ExampleScaleColor(rgb, 1.90f, 48);
            color::Text = 0xF3F3F5;
            color::TextMuted = 0x9C9DA5;
            color::TextDim = 0x6E6F77;
        }
    }

    static U32 Mix(U32 dst, U32 src, int a) noexcept {
        if (a <= 0) return dst;
        if (a >= 256) return src;
        const int db = dst & 0xFF, dg = (dst >> 8) & 0xFF, dr = (dst >> 16) & 0xFF;
        const int sb = src & 0xFF, sg = (src >> 8) & 0xFF, sr = (src >> 16) & 0xFF;
        const int b = db + ((sb - db) * a >> 8);
        const int g = dg + ((sg - dg) * a >> 8);
        const int r = dr + ((sr - dr) * a >> 8);
        return (U32)((dst & 0xFF000000u) | b | (g << 8) | (r << 16));
    }
    static U32 LerpColor(U32 a, U32 b, float t) noexcept {
        t = std::clamp(t, 0.f, 1.f);
        auto L = [t](int x, int y) { return (int)(x + (y - x) * t + 0.5f); };
        return (U32)(L(a & 0xFF, b & 0xFF)
            | (L((a >> 8) & 0xFF, (b >> 8) & 0xFF) << 8)
            | (L((a >> 16) & 0xFF, (b >> 16) & 0xFF) << 16));
    }

    class Surface {
    public:
        Surface() = default;
        ~Surface() { Release(); }
        Surface(const Surface&) = delete;
        Surface& operator=(const Surface&) = delete;

        bool Resize(int w, int h) {
            w = (std::max)(1, w); h = (std::max)(1, h);
            if (w == w_ && h == h_ && bits_) return true;
            Release();
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            void* p = nullptr;
            HDC screen = GetDC(nullptr);
            dib_ = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
            ReleaseDC(nullptr, screen);
            if (!dib_) return false;
            memdc_ = CreateCompatibleDC(nullptr);
            oldbmp_ = SelectObject(memdc_, dib_);
            bits_ = static_cast<U32*>(p);
            w_ = w; h_ = h; stride_ = w;
            ResetClip();
            return true;
        }
        void Release() {
            if (memdc_) { if (oldbmp_) SelectObject(memdc_, oldbmp_); DeleteDC(memdc_); memdc_ = nullptr; }
            if (dib_) { DeleteObject(dib_); dib_ = nullptr; }
            bits_ = nullptr; oldbmp_ = nullptr; w_ = h_ = stride_ = 0;
            cx0_ = cy0_ = 0; cx1_ = cy1_ = 0;
        }

        int  Width()  const noexcept { return w_; }
        int  Height() const noexcept { return h_; }
        HDC  DC()     const noexcept { return memdc_; }
        void SetSuppress(bool v) noexcept { suppress_ = v; }
        bool Suppressed() const noexcept { return suppress_; }

        void SetClip(int x0, int y0, int x1, int y1) noexcept {
            cx0_ = (std::max)(0, (std::min)(w_, x0));
            cy0_ = (std::max)(0, (std::min)(h_, y0));
            cx1_ = (std::max)(cx0_, (std::min)(w_, x1));
            cy1_ = (std::max)(cy0_, (std::min)(h_, y1));
        }
        void ResetClip() noexcept { cx0_ = 0; cy0_ = 0; cx1_ = w_; cy1_ = h_; }
        int ClipX0() const noexcept { return cx0_; }
        int ClipY0() const noexcept { return cy0_; }
        int ClipX1() const noexcept { return cx1_; }
        int ClipY1() const noexcept { return cy1_; }

        void Clear(U32 c) noexcept { if (suppress_) return; if (bits_) std::fill_n(bits_, (size_t)w_ * h_, c); }

        void FillRect(float X, float Y, float W, float H, U32 c, int alpha = 256) noexcept {
            if (suppress_) return;
            int x0 = (std::max)(0, (int)std::lround(X));
            int y0 = (std::max)(0, (int)std::lround(Y));
            int x1 = (std::min)(w_, (int)std::lround(X + W));
            int y1 = (std::min)(h_, (int)std::lround(Y + H));
            x0 = (std::max)(x0, cx0_); y0 = (std::max)(y0, cy0_);
            x1 = (std::min)(x1, cx1_); y1 = (std::min)(y1, cy1_);
            if (x1 <= x0 || y1 <= y0) return;
            for (int y = y0; y < y1; ++y) {
                U32* row = bits_ + (size_t)y * stride_ + x0;
                if (alpha >= 256) std::fill_n(row, x1 - x0, c);
                else for (int x = x0; x < x1; ++x) row[x - x0] = Mix(row[x - x0], c, alpha);
            }
        }

        void FillRoundRect(float X, float Y, float W, float H, float R,
            U32 c, int alpha = 256) noexcept {
            if (suppress_) return;
            const int x = (int)std::lround(X), y = (int)std::lround(Y);
            const int w = (int)std::lround(W), h = (int)std::lround(H);
            int ri = (int)std::lround(R);
            ri = (std::min)(ri, (std::min)(w, h) / 2);
            if (w <= 0 || h <= 0) return;
            if (ri <= 0) { FillRect((float)x, (float)y, (float)w, (float)h, c, alpha); return; }

            FillRect((float)(x + ri), (float)y, (float)(w - 2 * ri), (float)h, c, alpha);
            FillRect((float)x, (float)(y + ri), (float)ri, (float)(h - 2 * ri), c, alpha);
            FillRect((float)(x + w - ri), (float)(y + ri), (float)ri, (float)(h - 2 * ri), c, alpha);

            const float r = (float)ri;
            const float inner = (r - 0.5f) * (r - 0.5f);
            const float outer = (r + 0.5f) * (r + 0.5f);
            for (int cy = 0; cy < 2; ++cy) {
                for (int cx = 0; cx < 2; ++cx) {
                    const int ox = cx ? (x + w - ri) : x;
                    const int oy = cy ? (y + h - ri) : y;
                    for (int j = 0; j < ri; ++j) {
                        const float uy = cy ? (j + 0.5f) : (j + 0.5f - r);
                        const float uy2 = uy * uy;
                        if (uy2 >= outer) continue;
                        for (int i = 0; i < ri; ++i) {
                            const float ux = cx ? (i + 0.5f) : (i + 0.5f - r);
                            const float q = ux * ux + uy2;
                            if (q >= outer) continue;
                            int a = alpha;
                            if (q > inner) {
                                const float d = std::sqrt(q) - r;
                                a = (int)(std::clamp(0.5f - d, 0.f, 1.f) * alpha + 0.5f);
                            }
                            if (a > 0) BlendPixel(ox + i, oy + j, c, a);
                        }
                    }
                }
            }
        }

        void RoundRect(float X, float Y, float W, float H, float R,
            U32 fill, U32 stroke, float bw = 1.f) noexcept {
            FillRoundRect(X, Y, W, H, R, stroke, 256);
            FillRoundRect(X + bw, Y + bw, W - 2 * bw, H - 2 * bw,
                (std::max)(0.f, R - bw), fill, 256);
        }

        void Line(float x0, float y0, float x1, float y1, U32 c,
            float width = 1.f, int alpha = 256) noexcept {
            if (suppress_) return;
            const int minx = (std::max)(0, (int)std::floor((std::min)(x0, x1) - width - 1));
            const int maxx = (std::min)(w_, (int)std::ceil((std::max)(x0, x1) + width + 1));
            const int miny = (std::max)(0, (int)std::floor((std::min)(y0, y1) - width - 1));
            const int maxy = (std::min)(h_, (int)std::ceil((std::max)(y0, y1) + width + 1));
            const float dx = x1 - x0, dy = y1 - y0;
            const float len2 = dx * dx + dy * dy;
            const float half = 0.5f + width * 0.5f;
            const float outer = half * half;
            const float inner = (half > 1.f) ? (half - 1.f) * (half - 1.f) : -1.f;
            for (int y = miny; y < maxy; ++y) {
                const float py = y + 0.5f;
                for (int x = minx; x < maxx; ++x) {
                    const float px = x + 0.5f;
                    float t = 0.f;
                    if (len2 > 0.f) t = std::clamp(((px - x0) * dx + (py - y0) * dy) / len2, 0.f, 1.f);
                    const float ex = px - (x0 + t * dx), ey = py - (y0 + t * dy);
                    const float d2 = ex * ex + ey * ey;
                    if (d2 >= outer) continue;
                    int a = alpha;
                    if (d2 > inner) {
                        const float d = std::sqrt(d2);
                        a = (int)(std::clamp(half - d, 0.f, 1.f) * alpha + 0.5f);
                    }
                    if (a > 0) BlendPixel(x, y, c, a);
                }
            }
        }

        void Circle(float cx, float cy, float r, U32 c, int alpha = 256) noexcept {
            FillRoundRect(cx - r, cy - r, r * 2.f, r * 2.f, r, c, alpha);
        }

        void VerticalGradient(U32 top, U32 bottom) noexcept {
            if (suppress_) return;
            for (int y = 0; y < h_; ++y) {
                const U32 c = LerpColor(top, bottom, (float)y / (std::max)(1, h_ - 1));
                std::fill_n(bits_ + (size_t)y * stride_, w_, c);
            }
        }

        void CopyRectTo(U32* dst, int dstStride,
            int x0, int y0, int x1, int y1) const noexcept {
            if (!bits_ || !dst) return;
            x0 = (std::max)(0, (std::min)(w_, x0)); x1 = (std::max)(x0, (std::min)(w_, x1));
            y0 = (std::max)(0, (std::min)(h_, y0)); y1 = (std::max)(y0, (std::min)(h_, y1));
            for (int y = y0; y < y1; ++y)
                std::memcpy(dst + (size_t)(y - y0) * dstStride,
                    bits_ + (size_t)y * stride_ + x0,
                    (size_t)(x1 - x0) * sizeof(U32));
        }

        void BlendRectFrom(const U32* prev, int prevStride,
            int x0, int y0, int x1, int y1, int alpha) noexcept {
            if (suppress_) return;
            if (!bits_ || !prev || alpha >= 256) return;
            x0 = (std::max)(0, (std::min)(w_, x0)); x1 = (std::max)(x0, (std::min)(w_, x1));
            y0 = (std::max)(0, (std::min)(h_, y0)); y1 = (std::max)(y0, (std::min)(h_, y1));
            for (int y = y0; y < y1; ++y) {
                U32* row = bits_ + (size_t)y * stride_ + x0;
                const U32* old = prev + (size_t)(y - y0) * prevStride;
                for (int x = 0; x < x1 - x0; ++x) row[x] = Mix(old[x], row[x], alpha);
            }
        }

    private:
        void BlendPixel(int x, int y, U32 c, int a) noexcept {
            if (suppress_) return;
            if ((unsigned)x >= (unsigned)w_ || (unsigned)y >= (unsigned)h_) return;
            if (x < cx0_ || y < cy0_ || x >= cx1_ || y >= cy1_) return;
            U32& d = bits_[(size_t)y * stride_ + x];
            if (a >= 256) d = c;
            else if (a > 0) d = Mix(d, c, a);
        }
        int w_ = 0, h_ = 0, stride_ = 0;
        int cx0_ = 0, cy0_ = 0, cx1_ = 0, cy1_ = 0;
        U32* bits_ = nullptr;
        bool suppress_ = false;
        HBITMAP dib_ = nullptr;
        HDC memdc_ = nullptr;
        HGDIOBJ oldbmp_ = nullptr;
    };

    static void EnablePerMonitorDpiV2() noexcept {
        using Fn = BOOL(WINAPI*)(void*);
        HMODULE u = GetModuleHandleW(L"user32.dll");
        if (!u) return;
        auto f = reinterpret_cast<Fn>(GetProcAddress(u, "SetProcessDpiAwarenessContext"));
        if (f) f(reinterpret_cast<void*>(static_cast<intptr_t>(-4)));
    }
    static UINT WindowDpi(HWND h) noexcept {
        using Fn = UINT(WINAPI*)(HWND);
        HMODULE u = GetModuleHandleW(L"user32.dll");
        if (u && h) {
            auto f = reinterpret_cast<Fn>(GetProcAddress(u, "GetDpiForWindow"));
            if (f) { const UINT d = f(h); if (d) return d; }
        }
        HDC dc = GetDC(nullptr);
        const UINT d = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(nullptr, dc);
        return d ? d : 96;
    }
    static void ApplyDarkTitleBar(HWND h) noexcept {
        using Fn = long (WINAPI*)(HWND, DWORD, const void*, DWORD);
        HMODULE d = LoadLibraryW(L"dwmapi.dll");
        if (!d) return;
        auto f = reinterpret_cast<Fn>(GetProcAddress(d, "DwmSetWindowAttribute"));
        if (f) {
            BOOL dark = TRUE;
            f(h, 20, &dark, (DWORD)sizeof(dark));
            int backdrop = 2;
            f(h, 38, &backdrop, (DWORD)sizeof(backdrop));
        }
        FreeLibrary(d);
    }

    class ExampleGUI {
    public:
        using DrawFn = std::function<void(ExampleGUI&)>;

        ExampleGUI() = default;
        ~ExampleGUI() { DestroyFonts(); }
        ExampleGUI(const ExampleGUI&) = delete;
        ExampleGUI& operator=(const ExampleGUI&) = delete;

        bool Create(HINSTANCE inst, const wchar_t* title, int logicalW, int logicalH) {
            EnablePerMonitorDpiV2();
            inst_ = inst;
            dpiScale_ = (float)WindowDpi(nullptr) / 96.f;
            if (dpiScale_ < 1.f) dpiScale_ = 1.f;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = &ExampleGUI::WndProcThunk;
            wc.hInstance = inst_;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
            wc.lpszClassName = L"ExampleFluentGUI_Window";
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

            const int w = (int)std::lround(logicalW * dpiScale_);
            const int h = (int)std::lround(logicalH * dpiScale_);
            hwnd_ = CreateWindowExW(0, wc.lpszClassName, title,
                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                nullptr, nullptr, inst_, this);
            if (!hwnd_) return false;

            dpiScale_ = (float)WindowDpi(hwnd_) / 96.f;
            if (dpiScale_ < 1.f) dpiScale_ = 1.f;
            BuildFonts();
            ApplyDarkTitleBar(hwnd_);
            ShowWindow(hwnd_, SW_SHOWNORMAL);
            UpdateWindow(hwnd_);
            return true;
        }

        void OnDraw(DrawFn fn) { onDraw_ = std::move(fn); }
        void RequestRedraw() const noexcept { if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE); }
        void EnableAutoRefresh(UINT ms) noexcept {
            if (hwnd_) timerId_ = SetTimer(hwnd_, 1, ms, nullptr);
        }
        void DisableAutoRefresh() noexcept {
            if (hwnd_ && timerId_) { KillTimer(hwnd_, timerId_); timerId_ = 0; }
        }

        int Run() {
            MSG msg{};
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            return (int)msg.wParam;
        }

        void SetAlwaysOnTop(bool on) noexcept {
            topmost_ = on;
            if (!hwnd_) return;
            SetWindowPos(hwnd_, on ? HWND_TOPMOST : HWND_NOTOPMOST,
                0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        bool IsAlwaysOnTop() const noexcept { return topmost_; }

        void ReassertTopmost() noexcept {
            if (!hwnd_ || !topmost_) return;
            SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        void EnableZOrderGuard(UINT ms) noexcept {
            if (!hwnd_) return;
            if (zTimerId_) KillTimer(hwnd_, zTimerId_);
            zTimerId_ = SetTimer(hwnd_, 2, ms, nullptr);
        }
        void DisableZOrderGuard() noexcept {
            if (hwnd_ && zTimerId_) { KillTimer(hwnd_, zTimerId_); zTimerId_ = 0; }
        }
        bool ZOrderGuardActive() const noexcept { return zTimerId_ != 0; }

        void ExampleBeginMask(int id, const wchar_t* name,
            float x, float y, float w, float h,
            bool isStatic, unsigned long long sig) {
            if (activeMask_ >= 0) ExampleEndMask();
            const float sc = dpiScale_;
            const int px0 = (std::max)(0, (int)std::lround(x * sc));
            const int py0 = (std::max)(0, (int)std::lround(y * sc));
            const int px1 = (std::min)(surf_.Width(), (int)std::lround((x + w) * sc));
            const int py1 = (std::min)(surf_.Height(), (int)std::lround((y + h) * sc));
            if (px1 <= px0 || py1 <= py0) {
                activeMask_ = -1;
                surf_.SetSuppress(false);
                return;
            }
            MaskRegion& r = MaskById(id);
            r.id = id;
            r.name = name ? name : L"";
            r.isStatic = isStatic;
            r.x0 = x; r.y0 = y; r.x1 = x + w; r.y1 = y + h;
            r.px0 = px0; r.py0 = py0; r.px1 = px1; r.py1 = py1;
            r.sig = sig;
            activeMask_ = id;
            const bool masked = maskOverlay_ && (isStatic || masksEnabled_) &&
                !menu_.open && !overlay_.active;
            if (masked) {
                ++r.skips; r.redrawn = false; ++frameSkipped_;
                surf_.SetSuppress(true);
            }
            else {
                ++r.redraws; r.redrawn = true; ++frameRedrawn_;
                surf_.SetSuppress(false);
            }
        }
        void ExampleEndMask() {
            activeMask_ = -1;
            surf_.SetSuppress(false);
        }
        void ExampleSetMaskOverlay(bool on) noexcept {
            maskOverlay_ = on;
            RequestRedraw();
        }
        bool ExampleMaskOverlay() const noexcept { return maskOverlay_; }
        void ExampleSetMasksEnabled(bool on) noexcept {
            masksEnabled_ = on;
            RequestRedraw();
        }
        bool ExampleMasksEnabled() const noexcept { return masksEnabled_; }
        void ExampleInvalidateMasks() noexcept {
            for (auto& r : masks_) r.redrawn = false;
        }
        unsigned long long ExamplePointerCell() const noexcept {
            if (mx_ < 0.f || my_ < 0.f) return 0;
            const int cx = (int)(mx_ / 12.f), cy = (int)(my_ / 12.f);
            return ((unsigned long long)(unsigned)cx << 32) |
                (unsigned long long)(unsigned)cy;
        }
        void ExampleMaskStats(int& total, int& skipped, int& redrawn) const noexcept {
            total = (int)masks_.size();
            skipped = frameSkipped_;
            redrawn = frameRedrawn_;
        }
        void ExampleExportMask(std::wstring& out) const {
            std::wstring s = L"[\n";
            wchar_t b[640];
            for (const auto& r : masks_) {
                std::swprintf(b, 640,
                    L"  {\"id\":%d,\"name\":\"%s\",\"rect\":[%.1f,%.1f,%.1f,%.1f],"
                    L"\"static\":%s,\"sig\":\"0x%016llX\",\"redraws\":%d,\"skips\":%d,"
                    L"\"last\":\"%s\"},\n",
                    r.id, r.name.c_str(), r.x0, r.y0, r.x1, r.y1,
                    r.isStatic ? L"true" : L"false", r.sig,
                    r.redraws, r.skips, r.redrawn ? L"DRAW" : L"SKIP");
                s += b;
            }
            if (s.size() >= 2 && s[s.size() - 2] == L',') {
                s.erase(s.size() - 2, 1);
            }
            s += L"]\n";
            out = s;
        }


        std::wstring WindowAboveTitle() const {
            if (!hwnd_) return L"";
            HWND a = GetWindow(hwnd_, GW_HWNDPREV);
            if (!a || !IsWindowVisible(a)) return L"";
            wchar_t b[256]{};
            GetWindowTextW(a, b, 256);
            if (!b[0]) return L"(无标题窗口)";
            return std::wstring(b);
        }
        std::wstring WindowAboveClass() const {
            if (!hwnd_) return L"";
            HWND a = GetWindow(hwnd_, GW_HWNDPREV);
            if (!a || !IsWindowVisible(a)) return L"";
            wchar_t b[256]{};
            GetClassNameW(a, b, 256);
            if (!b[0]) return L"";
            return std::wstring(b);
        }

        HWND Handle() const noexcept { return hwnd_; }
        float ContentWidth()  const noexcept { return (float)surf_.Width() / dpiScale_; }
        float ContentHeight() const noexcept { return (float)surf_.Height() / dpiScale_; }
        float DpiScale()      const noexcept { return dpiScale_; }

        int  ExampleFrameCount()  const noexcept { return frameCount_; }
        bool ExampleConnected()   const noexcept { return connected_; }
        void ExampleSetConnected(bool v) { connected_ = v; RequestRedraw(); }

        void Begin() {
            surf_.ResetClip();
            const auto now = std::chrono::steady_clock::now();
            float dt = 0.f;
            if (lastFrame_ != std::chrono::steady_clock::time_point{})
                dt = std::chrono::duration<float>(now - lastFrame_).count();
            lastFrame_ = now;
            frameDt_ = std::clamp(dt, 1.f / 240.f, 0.05f);
            animActive_ = false;
            if (pageFadeActive_) {
                if (kPageFadeSec <= 0.f) {
                    pageFadeT_ = 1.f;
                    pageFadeActive_ = false;
                    pageAlpha_ = 256.f;
                    pageSlide_ = 0.f;
                }
                else {
                    pageFadeT_ += frameDt_ / kPageFadeSec;
                    if (pageFadeT_ >= 1.f) { pageFadeT_ = 1.f; pageFadeActive_ = false; }
                    const float u = 1.f - pageFadeT_;
                    const float e = 1.f - u * u * u;
                    pageAlpha_ = e * 256.f;
                    pageSlide_ = (1.f - e) * kPageSlidePx;
                    animActive_ = pageFadeActive_;
                }
            }
            else {
                pageAlpha_ = 256.f;
                pageSlide_ = 0.f;
            }
            if (std::fabs(builtFontScale_ - kFontScale) > 0.001f ||
                std::fabs(builtDpi_ - dpiScale_) > 0.001f) {
                BuildFonts();
                gdiDirty_ = true;
            }
            if (gdiDirty_) {
                SetBkMode(surf_.DC(), TRANSPARENT);
                curFont_ = nullptr;
                curColor_ = 0xFFFFFFFFu;
                gdiDirty_ = false;
            }
            frameClick_ = clickPending_;  clickPending_ = false;
            frameRClick_ = rclickPending_; rclickPending_ = false;
            menu_.justOpened = false;
            clickConsumed_ = false;
            rclickConsumed_ = false;
            frameWheel_ = wheelAccum_;    wheelAccum_ = 0;
            itemRectCount_ = 0;
            itemRectId_ = 0;
            frameChars_.swap(charQueue_);  charQueue_.clear();
            frameKeys_.swap(keyQueue_);    keyQueue_.clear();
            if (frameClick_) activeId_ = 0;
            if (!mouseDown_) { sliderDragId_ = 0; if (sliderEditId_ && activeId_ != sliderEditId_) sliderEditId_ = 0; }
            idCounter_ = 0;
            ++frameCount_;
            if (menu_.open && menu_.w > 0.f && menu_.h > 0.f) {
                menuModal_ = true;
                openDropdown_ = 0;
                blockHit_ = true;
                blockX_ = menu_.x; blockY_ = menu_.y;
                blockW_ = menu_.w; blockH_ = menu_.h;
            }
            else if (overlay_.active && openDropdown_ != 0) {
                menuModal_ = false;
                blockHit_ = true;
                blockX_ = overlay_.x; blockY_ = overlay_.y;
                blockW_ = overlay_.w;
                blockH_ = (float)overlay_.count * 28.f + 8.f;
            }
            else {
                menuModal_ = false;
                blockHit_ = false;
            }
            overlay_.Reset();

            frameSkipped_ = 0;
            frameRedrawn_ = 0;
            for (auto& r : masks_) r.redrawn = false;
            surf_.SetSuppress(false);
            activeMask_ = -1;
            surf_.VerticalGradient(color::BgTop, color::BgBottom);
            surf_.Line(0.f, 0.5f, (float)surf_.Width(), 0.5f,
                LerpColor(color::Accent, color::BgTop, 0.72f), 1.f, 90);

            layout_.clear();
            Region r{}; r.x = 0; r.y = 0;
            r.w = ContentWidth(); r.h = ContentHeight();
            r.cy = 0; r.cx = 0; r.rowH = 0; r.inRow = false;
            layout_.push_back(r);
        }

        void End() {
            ExampleEndMask();
            const int rw = pagePxX1_ - pagePxX0_;
            const int rh = pagePxY1_ - pagePxY0_;
            if (rw > 0 && rh > 0) {
                EnsurePrev(rw, rh);
                if (pageAlpha_ < 256.f && prevValid_ && !maskOverlay_)
                    surf_.BlendRectFrom(prevPage_.data(), prevStride_,
                        pagePxX0_, pagePxY0_, pagePxX1_, pagePxY1_,
                        (int)pageAlpha_);
            }

            DrawOverlay();
            HandleItemRightClick();
            DrawContextMenu();

            if (rw > 0 && rh > 0 && !pageFadeActive_ &&
                !menu_.open && !overlay_.active && !prevPage_.empty() &&
                !maskOverlay_) {
                surf_.CopyRectTo(prevPage_.data(), prevStride_,
                    pagePxX0_, pagePxY0_, pagePxX1_, pagePxY1_);
                prevValid_ = true;
            }

            if (animActive_ || pageFadeActive_) {
                if (hwnd_ && !animTimerId_)
                    animTimerId_ = SetTimer(hwnd_, 1001, 16, nullptr);
            }
            else if (animTimerId_) {
                KillTimer(hwnd_, animTimerId_);
                animTimerId_ = 0;
            }
            DrawMaskOverlay();
        }

        void ExampleBeginPanel(const wchar_t* title, float X, float Y, float W, float H) {
            const float s = dpiScale_;
            const float x = X * s, y = Y * s, w = W * s, h = H * s;
            surf_.RoundRect(x, y, w, h, kPanelR * s, color::Panel, color::PanelStroke, 1.f * s);
            surf_.FillRect(x + 1.f * s, y + 1.f * s, w - 2.f * s, 1.f * s,
                LerpColor(color::Text, color::Panel, 0.88f), 60);
            if (title && *title) {
                Text(x + kPanelPad * s, y + 8.f * s, w - 2 * kPanelPad * s, 20.f * s,
                    title, color::Text, Font::UIBold, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                surf_.Line(x + kPanelPad * s, y + 34.f * s, x + w - kPanelPad * s, y + 34.f * s,
                    color::PanelStroke, 1.f * s, 200);
            }
            Region r{}; r.x = X; r.y = Y; r.w = W; r.h = H;
            r.cy = Y + (title && *title ? 44.f : kPanelPad);
            r.cx = X + kPanelPad; r.rowH = 0; r.inRow = false;
            layout_.push_back(r);
        }
        void ExampleEndPanel() { if (layout_.size() > 1) layout_.pop_back(); }

        void ExampleBeginHorizontal() {
            Region& L = layout_.back();
            L.inRow = true; L.cx = L.x + L.pad; L.rowH = 0.f;
        }
        void ExampleEndHorizontal() {
            Region& L = layout_.back();
            if (L.inRow) { L.cy += L.rowH + kGap; L.inRow = false; L.rowH = 0.f; }
        }

        void ExampleTitleBar(const wchar_t* title, const wchar_t* subtitle) {
            const float s = dpiScale_, W = ContentWidth();
            Text(kPanelPad * s, 6.f * s, (W - 2 * kPanelPad) * s, 26.f * s, title,
                color::Text, Font::UIBoldLarge, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if (subtitle && *subtitle)
                Text(kPanelPad * s, 30.f * s, (W - 2 * kPanelPad) * s, 16.f * s, subtitle,
                    color::TextMuted, Font::UISmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ExampleSpace(6.f);
        }
        void ExampleSpace(float h = -1.f) {
            layout_.back().cy += (h < 0.f) ? kGap : h;
        }
        void ExampleSeparator() {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 1.f)) return;
            surf_.Line(x * s, y * s, (x + w) * s, y * s, color::PanelStroke, 1.f * s, 170);
        }
        void ExampleLabel(const wchar_t* text, bool muted = false) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 20.f)) return;
            Text(x * s, y * s, w * s, h * s, text,
                muted ? color::TextMuted : color::Text, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        void ExampleBadge(const wchar_t* text, U32 tint = 0xFFFFFFFFu) {
            if (tint == 0xFFFFFFFFu) tint = color::Accent;
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 74.f, 22.f)) return;
            surf_.FillRoundRect(x * s, y * s, w * s, h * s, 11.f * s, tint, 42);
            Text(x * s, y * s, w * s, h * s, text, tint, Font::UISmall,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        bool ExampleButton(const wchar_t* label, bool accent = false, float width = 0.f) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, width > 0 ? width : 0.f, kCtrlH)) return false;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) clickConsumed_ = true;
            U32 fill = accent ? (hov ? LerpColor(color::Accent, color::Text, 0.10f) : color::Accent)
                : (hov ? color::ControlHover : color::Control);
            if (mouseDown_ && hov) fill = accent ? color::AccentPress : color::ControlPress;
            U32 edge = accent ? LerpColor(color::Accent, color::Text, 0.15f) : color::Stroke;
            surf_.RoundRect(x * s, y * s, w * s, h * s, kCtrlR * s, fill, edge, 1.f * s);
            Text(x * s + 6.f * s, y * s, (w - 12.f) * s, h * s, label,
                accent ? color::OnAccent : color::Text, Font::UI,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return hit;
        }
        bool ExampleToggle(const wchar_t* label, bool& value) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, kCtrlH)) return false;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) { clickConsumed_ = true; value = !value; }
            Text(x * s, y * s, (w - 52.f) * s, h * s, label, color::Text, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const float tx = x + w - 44.f, ty = y + 6.f, tw = 40.f, th = 20.f;
            U32 track = value ? color::Accent : (hov ? color::ControlHover : color::Control);
            surf_.FillRoundRect(tx * s, ty * s, tw * s, th * s, 10.f * s, track, 256);
            if (!value) surf_.FillRoundRect(tx * s + 1.f * s, ty * s + 1.f * s,
                (tw - 2.f) * s, (th - 2.f) * s, 9.f * s,
                color::Stroke, 256);
            const float kd = value ? 12.f : 10.f;
            surf_.Circle((value ? tx + tw - 11.f : tx + 9.f) * s, (ty + th * 0.5f) * s,
                kd * 0.5f * s, value ? color::Text : color::TextMuted);
            return hit;
        }
        bool ExampleCheckbox(const wchar_t* label, bool& value) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 28.f)) return false;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) { clickConsumed_ = true; value = !value; }
            const float bx = x, by = y + 4.f, bs = 20.f;
            surf_.RoundRect(bx * s, by * s, bs * s, bs * s, 4.f * s,
                value ? color::Accent : (hov ? color::ControlHover : color::Control),
                value ? color::Accent : color::Stroke, 1.f * s);
            if (value) {
                surf_.Line((bx + 4.5f) * s, (by + 10.f) * s, (bx + 8.5f) * s, (by + 14.f) * s,
                    color::OnAccent, 2.f * s);
                surf_.Line((bx + 8.5f) * s, (by + 14.f) * s, (bx + 16.f) * s, (by + 6.f) * s,
                    color::OnAccent, 2.f * s);
            }
            Text((x + 28.f) * s, y * s, (w - 28.f) * s, h * s, label, color::Text, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return hit;
        }
        bool ExampleRadio(const wchar_t* label, int value, int& current) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 26.f)) return false;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) { clickConsumed_ = true; current = value; }
            const float cx = x + 10.f, cy = y + 13.f;
            const bool on = (current == value);
            surf_.FillRoundRect((cx - 9.f) * s, (cy - 9.f) * s, 18.f * s, 18.f * s, 9.f * s,
                on ? color::Accent : color::Control, 256);
            surf_.FillRoundRect((cx - 8.f) * s, (cy - 8.f) * s, 16.f * s, 16.f * s, 8.f * s,
                on ? color::Accent : color::Panel, 256);
            if (on) surf_.Circle(cx * s, cy * s, 4.f * s, color::OnAccent);
            Text((x + 28.f) * s, y * s, (w - 28.f) * s, h * s, label,
                on ? color::Text : color::TextMuted, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return hit;
        }
        static void FormatSlider(wchar_t* out, std::size_t cap, float v) noexcept {
            std::swprintf(out, cap, L"%.2f", (double)v);
        }
        static bool ParseSlider(const std::wstring& t, float& out) noexcept {
            if (t.empty()) return false;
            std::size_t i = 0;
            bool neg = false;
            if (t[i] == L'+' || t[i] == L'-') { neg = (t[i] == L'-'); ++i; }
            if (i >= t.size()) return false;
            double val = 0.0;
            bool any = false;
            while (i < t.size() && t[i] >= L'0' && t[i] <= L'9') {
                val = val * 10.0 + (double)(t[i] - L'0'); any = true; ++i;
            }
            if (i < t.size() && t[i] == L'.') {
                ++i;
                double frac = 0.1;
                while (i < t.size() && t[i] >= L'0' && t[i] <= L'9') {
                    val += (double)(t[i] - L'0') * frac;
                    frac *= 0.1; any = true; ++i;
                }
            }
            if (!any) return false;
            if (i < t.size() && (t[i] == L'e' || t[i] == L'E')) {
                ++i;
                bool eneg = false;
                if (i < t.size() && (t[i] == L'+' || t[i] == L'-')) {
                    eneg = (t[i] == L'-'); ++i;
                }
                int exp = 0;
                bool expAny = false;
                while (i < t.size() && t[i] >= L'0' && t[i] <= L'9') {
                    exp = exp * 10 + (int)(t[i] - L'0'); expAny = true; ++i;
                }
                if (!expAny) return false;
                val *= std::pow(10.0, (double)(eneg ? -exp : exp));
            }
            if (i != t.size() || !std::isfinite(val)) return false;
            out = (float)(neg ? -val : val);
            return true;
        }

        bool ExampleSlider(const wchar_t* label, float& value, float vmin, float vmax) {
            const float s = dpiScale_;
            const int id = ++idCounter_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 42.f)) return false;

            wchar_t hint[48];
            std::swprintf(hint, 48, L"%.2f ~ %.2f", (double)vmin, (double)vmax);
            Text(x * s, y * s, w * s, 16.f * s, label, color::TextMuted, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            Text(x * s, y * s, w * s, 16.f * s, hint, color::TextDim, Font::UISmall,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            const float boxW = std::clamp(kSliderInputW, 44.f, (std::max)(44.f, w * 0.4f));
            const float bx = x, by = y + 18.f, bh = 24.f;
            const float tx = x + boxW + 10.f, tw = (std::max)(24.f, w - boxW - 10.f);
            const float ty = by + bh * 0.5f - 2.f, th = 4.f;

            const bool boxHov = Hit(bx, by, boxW, bh);
            if (frameClick_ && !clickConsumed_ && boxHov) {
                clickConsumed_ = true;
                activeId_ = id;
                sliderEditId_ = id;
                sliderEditOrig_ = value;
                wchar_t tmp[32];
                FormatSlider(tmp, 32, value);
                sliderEditBuf_ = tmp;
                sliderEditFresh_ = true;
            }
            bool editing = (activeId_ == id && sliderEditId_ == id);
            if (!editing && sliderEditId_ == id) sliderEditId_ = 0;

            const bool trackHov = Hit(tx - 8.f, ty - 12.f, tw + 16.f, th + 24.f);
            if (frameClick_ && !clickConsumed_ && trackHov) {
                clickConsumed_ = true;
                sliderDragId_ = id;
            }
            if (!mouseDown_ && sliderDragId_ == id) sliderDragId_ = 0;

            bool changed = false;
            if (mouseDown_ && sliderDragId_ == id) {
                const float nt = std::clamp((mx_ - tx) / (std::max)(1.f, tw), 0.f, 1.f);
                const float nv = vmin + (vmax - vmin) * nt;
                if (nv != value) { value = nv; changed = true; }
                if (editing) {
                    wchar_t tmp[32];
                    FormatSlider(tmp, 32, value);
                    sliderEditBuf_ = tmp;
                    sliderEditFresh_ = false;
                }
            }

            if (editing) {
                bool cancelled = false;
                for (wchar_t ch : frameChars_) {
                    if (ch < 0x20 || ch == 0x7F) continue;
                    const bool ok = (ch >= L'0' && ch <= L'9') || ch == L'.' ||
                        ch == L'-' || ch == L'+' || ch == L'e' || ch == L'E';
                    if (ok && sliderEditBuf_.size() < 16) {
                        if (sliderEditFresh_) { sliderEditBuf_.clear(); sliderEditFresh_ = false; }
                        sliderEditBuf_.push_back(ch);
                        changed = true;
                    }
                }
                for (int vk : frameKeys_) {
                    if (vk == VK_BACK) {
                        sliderEditFresh_ = false;
                        if (!sliderEditBuf_.empty()) { sliderEditBuf_.pop_back(); changed = true; }
                    }
                    else if (vk == VK_RETURN || vk == VK_TAB) {
                        activeId_ = 0; sliderEditId_ = 0; editing = false;
                    }
                    else if (vk == VK_ESCAPE) {
                        activeId_ = 0; sliderEditId_ = 0; editing = false;
                        cancelled = true;
                        if (value != sliderEditOrig_) { value = sliderEditOrig_; changed = true; }
                    }
                }
                if (!cancelled) {
                    float parsed = 0.f;
                    if (ParseSlider(sliderEditBuf_, parsed)) {
                        const float nv = std::clamp(parsed, vmin, vmax);
                        if (nv != value) { value = nv; changed = true; }
                    }
                }
            }

            const bool focus = editing;
            surf_.RoundRect(bx * s, by * s, boxW * s, bh * s, kCtrlR * s,
                focus ? color::Panel : color::Control,
                focus ? color::Accent : (boxHov ? color::Stroke : color::PanelStroke),
                focus ? 1.5f * s : 1.f * s);
            wchar_t num[32];
            FormatSlider(num, 32, value);
            std::wstring shown = focus ? sliderEditBuf_ + L"|" : std::wstring(num);
            Text((bx + (focus ? 7.f : 0.f)) * s, by * s,
                (boxW - (focus ? 10.f : 0.f)) * s, bh * s, shown.c_str(),
                color::Text, Font::UISmall,
                focus ? (DT_LEFT | DT_VCENTER | DT_SINGLELINE)
                : (DT_CENTER | DT_VCENTER | DT_SINGLELINE));

            surf_.FillRoundRect(tx * s, ty * s, tw * s, th * s, 2.f * s,
                color::ControlHover, 256);
            const float t = (vmax > vmin) ? std::clamp((value - vmin) / (vmax - vmin), 0.f, 1.f) : 0.f;
            surf_.FillRoundRect(tx * s, ty * s, tw * t * s, th * s, 2.f * s, color::Accent, 256);
            const float hx = tx + tw * t;
            surf_.Circle(hx * s, (ty + th * 0.5f) * s, 7.f * s, color::Text);
            surf_.Circle(hx * s, (ty + th * 0.5f) * s, 5.f * s, color::Accent);
            return changed;
        }
        bool ExampleInput(const wchar_t* label, std::wstring& buf) {
            const float s = dpiScale_;
            const int id = ++idCounter_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 50.f)) return false;
            Text(x * s, y * s, w * s, 16.f * s, label, color::TextMuted, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const float fy = y + 20.f, fh = 30.f;
            const bool hov = Hit(x, fy, w, fh);
            if (frameClick_ && !clickConsumed_ && hov) { clickConsumed_ = true; activeId_ = id; }
            const bool focused = (activeId_ == id);
            surf_.RoundRect(x * s, fy * s, w * s, fh * s, kCtrlR * s,
                focused ? color::Panel : color::Control,
                focused ? color::Accent : (hov ? color::Stroke : color::PanelStroke),
                focused ? 1.5f * s : 1.f * s);
            std::wstring shown = buf.empty() ? std::wstring(L"") : buf;
            if (focused) shown += L"|";
            Text((x + 10.f) * s, fy * s, (w - 20.f) * s, fh * s, shown.c_str(),
                buf.empty() ? color::TextDim : color::Text, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            bool changed = false;
            if (focused) {
                for (wchar_t ch : frameChars_) {
                    if (ch >= 0x20 && ch != 0x7F) { buf.push_back(ch); changed = true; }
                }
                for (int vk : frameKeys_) {
                    if (vk == VK_BACK) { if (!buf.empty()) { buf.pop_back(); changed = true; } }
                    else if (vk == VK_RETURN || vk == VK_TAB || vk == VK_ESCAPE) activeId_ = 0;
                }
            }
            return changed;
        }
        bool ExampleDropdown(const wchar_t* label, int& index,
            std::initializer_list<const wchar_t*> items) {
            const float s = dpiScale_;
            const int id = ++idCounter_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 50.f)) return false;
            Text(x * s, y * s, w * s, 16.f * s, label, color::TextMuted, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const float fy = y + 20.f, fh = 30.f;
            const bool hov = Hit(x, fy, w, fh);
            if (frameClick_ && !clickConsumed_ && hov) {
                clickConsumed_ = true;
                openDropdown_ = (openDropdown_ == id) ? 0 : id;
            }
            surf_.RoundRect(x * s, fy * s, w * s, fh * s, kCtrlR * s,
                hov ? color::ControlHover : color::Control,
                openDropdown_ == id ? color::Accent : color::Stroke, 1.f * s);
            const wchar_t* cur = (index >= 0 && index < (int)items.size())
                ? *(items.begin() + index) : L"";
            if (!cur) cur = L"";
            Text((x + 10.f) * s, fy * s, (w - 34.f) * s, fh * s, cur, color::Text, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const float ax = x + w - 18.f, ay = fy + fh * 0.5f;
            surf_.Line((ax - 4.f) * s, (ay - 2.f) * s, ax * s, (ay + 2.f) * s, color::TextMuted, 1.5f * s);
            surf_.Line(ax * s, (ay + 2.f) * s, (ax + 4.f) * s, (ay - 2.f) * s, color::TextMuted, 1.5f * s);
            if (openDropdown_ == id) {
                overlay_.active = true;
                overlay_.owner = id;
                overlay_.x = x; overlay_.y = fy + fh + 3.f;
                overlay_.w = w; overlay_.target = &index;
                overlay_.count = 0;
                for (const wchar_t* it : items) {
                    if (overlay_.count >= (int)overlay_.items.size()) break;
                    overlay_.items[overlay_.count].assign(it ? it : L"");
                    ++overlay_.count;
                }
            }
            return false;
        }
        void ExampleProgressBar(const wchar_t* label, float value) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 38.f)) return;
            Text(x * s, y * s, w * s, 16.f * s, label, color::TextMuted, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const float t = std::clamp(value, 0.f, 1.f);
            const float by = y + 22.f, bh = 8.f;
            surf_.FillRoundRect(x * s, by * s, w * s, bh * s, 4.f * s, color::ControlHover, 256);
            if (t > 0.f) surf_.FillRoundRect(x * s, by * s, w * t * s, bh * s, 4.f * s, color::Accent, 256);
        }

        void ExampleHexView(const wchar_t* title, const std::uint8_t* data,
            std::size_t size, std::uint64_t base, float H) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, H)) return;
            if (title && *title)
                Text(x * s, y * s, w * s, 18.f * s, title, color::TextMuted, Font::UISmall,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const float top = y + (title && *title ? 20.f : 0.f);
            (void)h;
            const float rowH = 18.f;
            const int rows = (int)((size + 15) / 16);
            surf_.FillRoundRect(x * s, top * s, w * s, rowH * s, 3.f * s, color::Header, 256);
            Text((x + 8.f) * s, top * s, (w - 16.f) * s, rowH * s,
                L"偏移  字节数据                                       字符",
                color::TextMuted, Font::Mono, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            const wchar_t* hex = L"0123456789ABCDEF";
            int first = 0, last = 0;
            VisibleRange(top + rowH, rowH, rows, first, last);
            for (int i = first; i < last; ++i) {
                const size_t r = (size_t)i;
                const float ry = top + (float)(i + 1) * rowH;
                if (i % 2 == 1) surf_.FillRect(x * s, ry * s, w * s, rowH * s, color::RowAlt, 90);
                wchar_t line[96]{};
                const std::uint8_t* p = data + r * 16;
                const std::uint64_t off = base + (std::uint64_t)r * 16;
                int n = 0;
                for (int k = 15; k >= 0; --k) line[n++] = hex[(int)((off >> (k * 4)) & 15)];
                line[n++] = L' '; line[n++] = L' ';
                for (int c = 0; c < 16; ++c) {
                    const bool ok = (r * 16 + (size_t)c < size);
                    const std::uint8_t b = ok ? p[c] : 0;
                    if (ok) { line[n++] = hex[b >> 4]; line[n++] = hex[b & 15]; }
                    else { line[n++] = L' '; line[n++] = L' '; }
                    line[n++] = L' ';
                    if (c == 7) line[n++] = L' ';
                }
                line[n++] = L' '; line[n++] = L'|';
                for (int c = 0; c < 16; ++c) {
                    const std::uint8_t b = (r * 16 + (size_t)c < size) ? p[c] : 0;
                    line[n++] = (b >= 0x20 && b < 0x7F) ? (wchar_t)b : L'.';
                }
                line[n] = L'\0';
                Text((x + 8.f) * s, ry * s, (w - 16.f) * s, rowH * s, line,
                    color::Text, Font::Mono, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        void ExampleLog(const wchar_t* title, const std::vector<std::wstring>& lines,
            float H) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, H)) return;
            if (title && *title)
                Text(x * s, y * s, w * s, 18.f * s, title, color::TextMuted, Font::UISmall,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            (void)h;
            const float top = y + (title && *title ? 20.f : 0.f);
            const int total = (int)lines.size();
            int first = 0, last = 0;
            VisibleRange(top, kRowH, total, first, last);
            for (int i = first; i < last; ++i) {
                const int idx = i;
                const float ry = top + (float)i * kRowH;
                if (i % 2 == 1) surf_.FillRect(x * s, ry * s, w * s, kRowH * s, color::RowAlt, 80);
                const std::wstring& t = lines[idx];
                U32 c = color::TextMuted;
                if (t.find(L"失败") != std::wstring::npos) c = color::Error;
                else if (t.find(L"警告") != std::wstring::npos) c = color::Warn;
                else if (t.find(L"完成") != std::wstring::npos) c = color::Ok;
                Text((x + 8.f) * s, ry * s, (w - 16.f) * s, kRowH * s, t.c_str(),
                    c, Font::Mono, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }
        void ExampleTable(const wchar_t* title,
            const std::vector<std::wstring>& headers,
            const std::vector<std::vector<std::wstring>>& rows, float H,
            bool showHeader = true,
            int* rightClickedRow = nullptr,
            int menuOwner = 0) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, H)) return;
            if (title && *title)
                Text(x * s, y * s, w * s, 18.f * s, title, color::TextMuted, Font::UISmall,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            (void)h;
            const float top = y + (title && *title ? 20.f : 0.f);
            const int cols = (int)headers.size();
            if (cols <= 0) return;
            const float cw = w / cols;
            const float headH = showHeader ? kRowH : 0.f;
            if (showHeader) {
                surf_.FillRoundRect(x * s, top * s, w * s, kRowH * s, 3.f * s, color::Header, 256);
                for (int c = 0; c < cols; ++c)
                    Text((x + c * cw + 8.f) * s, top * s, (cw - 12.f) * s, kRowH * s,
                        headers[c].c_str(), color::Text, Font::UIBold,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            int first = 0, last = 0;
            VisibleRange(top + headH, kRowH, (int)rows.size(), first, last);
            for (int i = first; i < last; ++i) {
                const float ry = top + headH + (float)i * kRowH;
                const bool rh = frameRClick_ && !rclickConsumed_ && !menu_.open &&
                    Hit(x, ry, w, kRowH);
                if (rh) {
                    rclickConsumed_ = true;
                    if (rightClickedRow) *rightClickedRow = i;
                    ExampleOpenContextMenu(menuOwner);
                }
                if (rh || i % 2 == 1)
                    surf_.FillRect(x * s, ry * s, w * s, kRowH * s,
                        rh ? color::Accent : color::RowAlt, rh ? 34 : 90);
                for (int c = 0; c < cols && c < (int)rows[i].size(); ++c)
                    Text((x + c * cw + 8.f) * s, ry * s, (cw - 12.f) * s, kRowH * s,
                        rows[i][c].c_str(), color::TextMuted, Font::UI,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }
        bool ExampleColorSwatch(const wchar_t* label, U32 c, bool selected,
            float bw = 104.f) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, bw, 30.f)) return false;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) clickConsumed_ = true;
            surf_.RoundRect(x * s, y * s, w * s, h * s, kCtrlR * s,
                hov ? color::ControlHover : color::Control,
                selected ? color::Accent : color::Stroke,
                selected ? kBorderWidth * s : 1.f * s);
            surf_.FillRoundRect((x + 6.f) * s, (y + 6.f) * s,
                18.f * s, (h - 12.f) * s, 3.f * s, c, 256);
            Text((x + 30.f) * s, y * s, (w - 36.f) * s, h * s, label,
                color::Text, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            return hit;
        }

        void ExampleStylePreview(float H = 96.f) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, H)) return;
            surf_.FillRoundRect(x * s, y * s, w * s, h * s, kPanelR * s, color::BgTop, 256);
            surf_.RoundRect((x + 10.f) * s, (y + 10.f) * s, (w - 20.f) * s, (h - 20.f) * s,
                kPanelR * s, color::Panel, color::PanelStroke, 1.f * s);
            const float cx = x + 22.f, cy = y + 22.f;
            surf_.FillRoundRect(cx * s, cy * s, 120.f * s, 26.f * s, kCtrlR * s,
                color::Control, 256);
            surf_.FillRoundRect((cx + 132.f) * s, cy * s, 120.f * s, 26.f * s, kCtrlR * s,
                color::Accent, 256);
            Text(cx * s, cy * s, 120.f * s, 26.f * s, L"控件",
                color::Text, Font::UI, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            Text((cx + 132.f) * s, cy * s, 120.f * s, 26.f * s, L"强调",
                color::OnAccent, Font::UI, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            Text((x + 22.f) * s, (y + 52.f) * s, (w - 44.f) * s, 18.f * s,
                L"文字预览 · 配色与字号实时生效",
                color::TextMuted, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            surf_.FillRoundRect((x + 22.f) * s, (y + 74.f) * s,
                (w - 44.f) * s, 6.f * s, 3.f * s, color::Control, 256);
            surf_.FillRoundRect((x + 22.f) * s, (y + 74.f) * s,
                (w - 44.f) * 0.62f * s, 6.f * s, 3.f * s, color::Accent, 256);
        }

        void ExampleBeginNavigation(float X, float Y, float W, float H) {
            const float s = dpiScale_;
            surf_.FillRect(X * s, Y * s, W * s, H * s, color::NavBg, 256);
            surf_.FillRect((X + W - 1.f) * s, Y * s, 1.f * s, H * s, color::PanelStroke, 200);
            Region r{}; r.x = X; r.y = Y; r.w = W; r.h = H;
            r.cy = Y + 8.f; r.cx = X + kPagePad; r.pad = kPagePad;
            r.rowH = 0.f; r.inRow = false;
            layout_.push_back(r);
        }
        void ExampleEndNavigation() { if (layout_.size() > 1) layout_.pop_back(); }

        bool ExampleNavItem(const wchar_t* label, int page, int& current,
            bool* rightClicked = nullptr) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, kNavItemH)) return false;
            layout_.back().cy += kNavGap - kGap;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) {
                clickConsumed_ = true;
                if (current != page) {
                    current = page;
                    StartPageTransition();
                }
            }
            const bool rclick = frameRClick_ && !rclickConsumed_ && !menu_.open &&
                Hit(x, y, w, h);
            if (rclick) {
                rclickConsumed_ = true;
                ExampleOpenContextMenu(1000 + page);
                if (rightClicked) *rightClicked = true;
            }
            const bool on = (current == page);
            const U32 bg = on ? color::NavActive : (hov ? color::ControlHover : color::NavBg);
            surf_.FillRect(x * s, y * s, w * s, h * s, bg, 256);
            if (on) surf_.FillRect(x * s, y * s, 3.f * s, h * s, color::Accent, 256);
            surf_.FillRect((x + 10.f) * s, (y + 11.f) * s, 12.f * s, 12.f * s,
                on ? color::Accent : color::TextDim, on ? 256 : 150);
            Text((x + 28.f) * s, y * s, (w - 34.f) * s, h * s, label,
                on ? color::Text : color::TextMuted, Font::UI,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            return hit;
        }

        struct ScrollGesture {
            bool dragging = false;
            float dragValue = 0.f;
            int pageDir = 0;
        };

        ScrollGesture PollScrollbar(float cur, float maxScroll,
            float X, float Y, float W, float H,
            float contentH, float viewH) {
            ScrollGesture g{};
            const float minThumb = 28.f;
            const float thumbH = (maxScroll <= 0.f)
                ? H
                : std::clamp(H * viewH / (std::max)(1.f, contentH),
                    minThumb, H);
            const float travel = (std::max)(1.f, H - thumbH);
            const float t = (maxScroll > 0.f) ? cur / maxScroll : 0.f;
            const float thumbY = Y + travel * t;
            const bool onTrack = RawHit(X - 3.f, Y, W + 6.f, H);
            const bool onThumb = RawHit(X - 3.f, thumbY, W + 6.f, thumbH);
            if (onTrack && frameClick_ && !clickConsumed_ && !menuModal_) {
                clickConsumed_ = true;
                if (onThumb) {
                    sbDrag_ = true;
                    sbDragOffset_ = my_ - thumbY;
                }
                else if (maxScroll > 0.f) {
                    g.pageDir = (my_ < thumbY) ? -1 : 1;
                }
            }
            if (!mouseDown_) sbDrag_ = false;
            if (sbDrag_ && maxScroll > 0.f) {
                g.dragging = true;
                g.dragValue = std::clamp(
                    ((my_ - sbDragOffset_ - Y) / travel) * maxScroll,
                    0.f, maxScroll);
            }
            return g;
        }

        void DrawScrollbar(float cur, float maxScroll,
            float X, float Y, float W, float H,
            float contentH, float viewH) {
            const float s = dpiScale_;
            surf_.FillRoundRect(X * s, Y * s, W * s, H * s, W * 0.5f * s,
                color::NavBg, 256);
            const float minThumb = 28.f;
            const float thumbH = (maxScroll <= 0.f)
                ? H
                : std::clamp(H * viewH / (std::max)(1.f, contentH),
                    minThumb, H);
            const float travel = (std::max)(1.f, H - thumbH);
            const float t = (maxScroll > 0.f) ? cur / maxScroll : 0.f;
            const float drawY = Y + travel * t;
            const bool hot = sbDrag_ || RawHit(X - 3.f, drawY, W + 6.f, thumbH);
            surf_.FillRoundRect(X * s, drawY * s, W * s, thumbH * s, W * 0.5f * s,
                hot ? color::Accent : color::ControlHover,
                hot ? 235 : 256);
            if (maxScroll > 0.f)
                surf_.FillRect((X + W * 0.32f) * s, (drawY + thumbH * 0.5f - 5.f) * s,
                    1.f * s, 10.f * s, color::TextDim, 120);
        }

        void ExampleBeginPage(float X, float Y, float W, float H,
            int* scroll = nullptr, float* measuredH = nullptr) {
            const float s = dpiScale_;
            const float viewY = Y + 8.f;
            const float viewH = (std::max)(40.f, H - 16.f);
            const float contentW = W - 16.f;
            float contentH = viewH;
            if (measuredH && *measuredH > 0.f) contentH = *measuredH;
            const float maxScroll = (std::max)(0.f, contentH - viewH);
            const float bx = X + W - 12.f;

            SmoothScroll dummy{};
            SmoothScroll& sm = scroll ? FindSmooth(scroll, (float)*scroll) : dummy;
            sm.target = std::clamp(sm.target, 0.f, maxScroll);
            sm.cur = std::clamp(sm.cur, 0.f, maxScroll);

            const ScrollGesture hit = PollScrollbar(sm.cur, maxScroll,
                bx, viewY, 8.f, viewH,
                contentH, viewH);
            if (!menu_.open && !openDropdown_ && frameWheel_ != 0 &&
                RawHit(X, Y, W, H))
                sm.target -= (float)frameWheel_ / 120.f * kScrollWheelLines * kRowH;
            if (hit.pageDir != 0)
                sm.target += (float)hit.pageDir * viewH * kScrollPageFraction;
            sm.target = std::clamp(sm.target, 0.f, maxScroll);

            if (hit.dragging) {
                sm.cur = hit.dragValue;
                sm.target = hit.dragValue;
            }
            else {
                const float k = 1.f - std::exp(-frameDt_ * kScrollEase);
                sm.cur += (sm.target - sm.cur) * k;
                if (std::fabs(sm.target - sm.cur) < 0.5f) sm.cur = sm.target;
            }
            if (sm.cur != sm.target) animActive_ = true;
            if (scroll) *scroll = (int)std::lround(sm.cur);

            surf_.SetClip((int)(X * s), (int)(viewY * s),
                (int)((X + W) * s), (int)((viewY + viewH) * s));
            DrawScrollbar(sm.cur, maxScroll, bx, viewY, 8.f, viewH, contentH, viewH);

            pagePxX0_ = (int)(X * s);
            pagePxY0_ = (int)(viewY * s);
            pagePxX1_ = (int)((X + W) * s);
            pagePxY1_ = (int)((viewY + viewH) * s);

            Region r{};
            r.x = X; r.y = Y; r.w = contentW; r.h = H;
            r.cy = viewY; r.cx = X + kPagePad; r.pad = kPagePad;
            r.scrollY = sm.cur;
            r.viewY = viewY; r.viewH = viewH; r.clipped = true;
            r.animX = pageSlide_;
            r.rowH = 0.f; r.inRow = false;
            layout_.push_back(r);
            pageItemStart_ = itemRectCount_;
            pageViewY_ = viewY;
            pageMeasuredPtr_ = measuredH;
        }

        void ExampleEndPage() {
            if (layout_.size() > 1) {
                Region& L = layout_.back();
                const float measured = (std::max)(0.f, L.cy - pageViewY_);
                if (pageMeasuredPtr_) {
                    if (std::fabs(*pageMeasuredPtr_ - measured) > 0.5f)
                        RequestRedraw();
                    *pageMeasuredPtr_ = measured;
                }
                layout_.pop_back();
            }
            surf_.ResetClip();
        }

        void HandleItemRightClick() {
            if (!frameRClick_ || rclickConsumed_ || menu_.open) return;
            for (int i = itemRectCount_ - 1; i >= pageItemStart_; --i) {
                const ItemRect& r = itemRects_[i];
                if (RawHit(r.x, r.y, r.w, r.h)) {
                    rclickConsumed_ = true;
                    ExampleOpenContextMenu(9000 + r.id);
                    menu_.hasHighlight = true;
                    menu_.highlight = r;
                    if (menu_.highlight.h > 60.f) {
                        menu_.highlight.y = my_ - 11.f;
                        menu_.highlight.h = 22.f;
                    }
                    return;
                }
            }
        }

        void ExampleColumnHeader(std::initializer_list<const wchar_t*> columns,
            float widthLimit = 0.f) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, kRowH)) return;
            if (widthLimit > 0.f) w = (std::min)(w, widthLimit);
            surf_.FillRect(x * s, y * s, w * s, h * s, color::Header, 256);
            const int n = (int)columns.size();
            if (n <= 0) return;
            const float cw = w / n;
            int i = 0;
            for (const wchar_t* c : columns) {
                Text((x + (float)i * cw + 10.f) * s, y * s, (cw - 14.f) * s, h * s,
                    c ? c : L"", color::TextMuted, Font::UISmall,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                ++i;
            }
        }

        void ExampleProcessCard(const ExampleProcessInfo& info, ExampleProcessState state,
            bool* rightClicked = nullptr, int menuOwner = 0) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, kCardH)) return;
            w = (std::min)(w, kCardW);
            const bool rh = frameRClick_ && !rclickConsumed_ && !menu_.open &&
                Hit(x, y, w, h);
            if (rh) {
                rclickConsumed_ = true;
                if (rightClicked) *rightClicked = true;
                ExampleOpenContextMenu(menuOwner);
            }
            U32 border = 0;
            bool framed = true;
            const wchar_t* tag = L"常态";
            switch (state) {
            case ExampleProcessState::ExampleNew:
                border = color::NewGreen; tag = L"新建"; break;
            case ExampleProcessState::ExampleLongRunning:
                framed = false; tag = L"常态"; break;
            case ExampleProcessState::ExampleTerminated:
                border = color::TerminatedRed; tag = L"已终止"; break;
            }
            const U32 bg = framed ? color::Control : color::Panel;
            if (framed) {
                surf_.FillRoundRect((x - 2.f) * s, (y - 2.f) * s, (w + 4.f) * s, (h + 4.f) * s,
                    (kCardR + 2.f) * s, border, 48);
                surf_.RoundRect(x * s, y * s, w * s, h * s, kCardR * s, bg, border, kBorderWidth * s);
            }
            else {
                surf_.FillRoundRect(x * s, y * s, w * s, h * s, kCardR * s, bg, 256);
            }
            if (rh) surf_.FillRoundRect((x + 2.f) * s, (y + 2.f) * s,
                (w - 4.f) * s, (h - 4.f) * s,
                4.f * s, color::Accent, 22);
            const float tw = 76.f, th = 20.f;
            const float tx = x + w - tw - 12.f, ty = y + (h - th) * 0.5f;
            surf_.FillRoundRect(tx * s, ty * s, tw * s, th * s, 10.f * s,
                framed ? border : color::ControlHover, framed ? 46 : 256);
            Text(tx * s, ty * s, tw * s, th * s, tag,
                framed ? border : color::TextMuted, Font::UISmall,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            const float lx = x + 12.f;
            const float aw = w - 24.f - tw - 12.f;
            const UINT one = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
            const float c0 = aw * 0.32f, c1 = aw * 0.13f;
            const float c2 = aw * 0.10f, c3 = aw * 0.13f;
            float cx = lx;
            Text(cx * s, y * s, (c0 - 6.f) * s, h * s,
                info.name.c_str(), color::Text, Font::UIBold, one);
            cx += c0;
            Text(cx * s, y * s, (c1 - 6.f) * s, h * s,
                info.pid.c_str(), color::TextMuted, Font::UI, one);
            cx += c1;
            Text(cx * s, y * s, (c2 - 6.f) * s, h * s,
                info.cpu.c_str(), color::TextMuted, Font::UI, one);
            cx += c2;
            Text(cx * s, y * s, (c3 - 6.f) * s, h * s,
                info.memory.c_str(), color::TextMuted, Font::UI, one);
            cx += c3;
            Text(cx * s, y * s, (lx + aw - cx - 4.f) * s, h * s,
                info.imagePath.c_str(), color::TextDim, Font::UISmall, one);
        }

        void ExampleOpenContextMenu(int owner) {
            menu_.open = true;
            menu_.owner = owner;
            menu_.justOpened = true;
            menu_.x = std::clamp(mx_, 2.f,
                (std::max)(2.f, ContentWidth() - menu_.w - 2.f));
            menu_.y = std::clamp(my_, 2.f,
                (std::max)(2.f, ContentHeight() - menu_.h - 2.f));
            menu_.clickedId = 0;
            RequestRedraw();
        }

        int ExampleContextMenu(std::initializer_list<ExampleMenuEntry> items) {
            const int result = menu_.clickedId;
            menu_.clickedId = 0;
            menu_.count = 0;
            float maxText = 0.f, maxShort = 0.f;
            for (const ExampleMenuEntry& e : items) {
                if (menu_.count >= (int)menu_.entries.size()) break;
                menu_.entries[menu_.count] = e;
                ++menu_.count;
                if (e.label) maxText = (std::max)(maxText, MeasureLabel(e.label));
                if (e.shortcut) maxShort = (std::max)(maxShort, MeasureLabel(e.shortcut));
            }
            menu_.w = std::clamp(14.f + maxText + 20.f + maxShort + 26.f, 190.f, 360.f);
            float hh = 12.f;
            for (int i = 0; i < menu_.count; ++i)
                hh += menu_.entries[i].label ? 30.f : 9.f;
            menu_.h = hh + 6.f;
            menu_.x = std::clamp(menu_.x, 2.f, (std::max)(2.f, ContentWidth() - menu_.w - 2.f));
            menu_.y = std::clamp(menu_.y, 2.f, (std::max)(2.f, ContentHeight() - menu_.h - 2.f));
            return result;
        }

        static constexpr float kTreeColW[7] = {
            0.20f, 0.08f, 0.07f, 0.08f, 0.14f, 0.20f, 0.23f
        };

        void ExampleTextAt(float X, float Y, float W, float H,
            const wchar_t* text, U32 c, int font, UINT flags) {
            Font f = Font::UI;
            switch (font) {
            case 1: f = Font::UIBold; break;
            case 2: f = Font::UIBoldLarge; break;
            case 3: f = Font::UISmall; break;
            case 4: f = Font::Mono; break;
            default: f = Font::UI; break;
            }
            const float s = dpiScale_;
            Text(X * s, Y * s, W * s, H * s, text, c, f, flags | DT_NOPREFIX);
        }

        void ExampleAbsRect(float X, float Y, float W, float H,
            U32 fill, U32 stroke) {
            const float s = dpiScale_;
            surf_.FillRect(X * s, Y * s, W * s, H * s, fill, 256);
            surf_.Line((X + W - 1.f) * s, Y * s, (X + W - 1.f) * s, (Y + H) * s,
                stroke, 1.f * s, 180);
            surf_.Line(X * s, (Y + H - 1.f) * s, (X + W) * s, (Y + H - 1.f) * s,
                stroke, 1.f * s, 180);
            surf_.Line(X * s, Y * s, (X + W) * s, Y * s, stroke, 1.f * s, 120);
        }

        void ExampleBeginAbs(float X, float Y, float W, float H, float pad = 4.f) {
            Region r{};
            r.x = X; r.y = Y; r.w = W; r.h = H;
            r.cy = Y + pad; r.cx = X + pad; r.pad = pad;
            r.rowH = 0.f; r.inRow = false;
            layout_.push_back(r);
        }
        void ExampleEndAbs() {
            if (layout_.size() > 1) layout_.pop_back();
        }

        void ExampleBeginClip(float X, float Y, float W, float H) {
            const float s = dpiScale_;
            surf_.SetClip((int)(X * s), (int)(Y * s),
                (int)((X + W) * s), (int)((Y + H) * s));
        }
        void ExampleEndClip() { surf_.ResetClip(); }

        bool ExampleSearchBox(const wchar_t* hint, std::wstring& buf, float width) {
            const float s = dpiScale_;
            const int id = ++idCounter_;
            float x, y, w, h;
            if (!Place(x, y, w, h, width, kCtrlH)) return false;
            const bool hov = Hit(x, y, w, h);
            if (frameClick_ && !clickConsumed_ && hov) {
                clickConsumed_ = true;
                activeId_ = id;
            }
            const bool focused = (activeId_ == id);
            surf_.RoundRect(x * s, y * s, w * s, h * s, kCtrlR * s,
                focused ? color::Panel : color::Control,
                focused ? color::Accent : (hov ? color::Stroke : color::PanelStroke),
                focused ? 1.5f * s : 1.f * s);
            std::wstring shown = buf.empty() ? std::wstring(hint ? hint : L"") : buf;
            if (focused) shown += L"|";
            Text((x + 10.f) * s, y * s, (w - 20.f) * s, h * s, shown.c_str(),
                buf.empty() ? color::TextDim : color::Text, Font::UISmall,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            bool changed = false;
            if (focused) {
                for (wchar_t ch : frameChars_) {
                    if (ch >= 0x20 && ch != 0x7F) { buf.push_back(ch); changed = true; }
                }
                for (int vk : frameKeys_) {
                    if (vk == VK_BACK) {
                        if (!buf.empty()) { buf.pop_back(); changed = true; }
                    }
                    else if (vk == VK_RETURN || vk == VK_TAB || vk == VK_ESCAPE) {
                        activeId_ = 0;
                    }
                }
            }
            return changed;
        }

        bool ExampleSideBarItem(const wchar_t* icon, const wchar_t* iconOn,
            const wchar_t* label, int page, int& current) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, 58.f)) return false;
            const bool hov = Hit(x, y, w, h);
            const bool hit = frameClick_ && !clickConsumed_ && hov;
            if (hit) clickConsumed_ = true;
            const bool on = (current == page);
            if (on || hov) {
                surf_.FillRoundRect(x * s, y * s, w * s, h * s, 6.f * s,
                    on ? color::NavActive : color::ControlHover,
                    on ? 256 : 70);
            }
            if (on) {
                surf_.FillRect((x + 1.f) * s, (y + 8.f) * s,
                    3.f * s, (h - 16.f) * s, color::Accent, 256);
            }
            const bool useIcons = ExampleIconFonts::Ready();
            const wchar_t* gi = useIcons ? (on ? (iconOn ? iconOn : icon) : icon) : icon;
            const Font gf = useIcons
                ? (on ? Font::IconFilled : Font::IconRegular)
                : Font::UIBoldLarge;
            Text(x * s, (y + 7.f) * s, w * s, 26.f * s, gi,
                on ? color::Accent : color::TextMuted, gf,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            Text(x * s, (y + 33.f) * s, w * s, 18.f * s, label,
                on ? color::Text : color::TextMuted, Font::UISmall,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (hit && current != page) {
                current = page;
                StartPageTransition();
            }
            return hit;
        }

        int ExampleSortHeader(std::initializer_list<const wchar_t*> columns,
            int sortCol, bool asc, float widthLimit = 0.f) {
            const float s = dpiScale_;
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, kRowH, 0.f)) return -1;
            if (widthLimit > 0.f) w = (std::min)(w, widthLimit);
            surf_.FillRect(x * s, y * s, w * s, h * s, color::Header, 256);
            surf_.Line(x * s, (y + h - 1.f) * s, (x + w) * s, (y + h - 1.f) * s,
                color::PanelStroke, 1.f * s, 200);
            const float iconSz = (std::min)(kProcIconSize, kRowH - 2.f);
            const float tx = x + 26.f + iconSz;
            const float avail = (x + w) - tx - 4.f;
            if (avail <= 0.f) return -1;
            int clicked = -1;
            int i = 0;
            float cx = tx;
            for (const wchar_t* c : columns) {
                if (i >= 7) break;
                const float cw = avail * kTreeColW[i];
                std::wstring t = (c ? std::wstring(c) : std::wstring());
                if (sortCol == i) t += asc ? L"  ▲" : L"  ▼";
                Text((cx + 6.f) * s, y * s, (cw - 8.f) * s, h * s, t.c_str(),
                    sortCol == i ? color::Accent : color::TextMuted,
                    Font::UISmall,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                surf_.Line((cx + cw) * s, (y + 4.f) * s,
                    (cx + cw) * s, (y + h - 4.f) * s,
                    color::PanelStroke, 1.f * s, 130);
                if (clicked < 0 && frameClick_ && !clickConsumed_ &&
                    Hit(cx, y, cw, h)) {
                    clickConsumed_ = true;
                    clicked = i;
                }
                cx += cw;
                ++i;
            }
            return clicked;
        }

        struct ExampleTreeHit {
            bool selected = false;
            bool toggled = false;
        };

        ExampleTreeHit ExampleTreeRow(
            const wchar_t* name, const wchar_t* pid, const wchar_t* cpu,
            const wchar_t* mem, const wchar_t* company, const wchar_t* desc,
            const wchar_t* path,
            int depth, bool hasChild, bool expanded,
            int state, bool selected,
            int* rightClickedRow, int rowIndex, int menuOwner,
            HICON icon = nullptr) {
            const float s = dpiScale_;
            ExampleTreeHit out{};
            float x, y, w, h;
            if (!Place(x, y, w, h, 0.f, kRowH, 0.f)) return out;
            if (rowIndex % 2 == 1)
                surf_.FillRect(x * s, y * s, w * s, h * s, color::RowAlt, 80);
            if (selected)
                surf_.FillRect(x * s, y * s, w * s, h * s, color::Accent, 34);

            const float bx = x + 1.f, by = y + 1.f;
            const float bw = w - 2.f, bh = h - 2.f;
            if (state == 1 || state == 2) {
                const U32 border = (state == 1) ? color::NewGreen : color::TerminatedRed;
                surf_.Line(bx * s, by * s, (bx + bw) * s, by * s,
                    border, 1.6f * s, 230);
                surf_.Line(bx * s, (by + bh) * s, (bx + bw) * s, (by + bh) * s,
                    border, 1.6f * s, 230);
                surf_.Line(bx * s, by * s, bx * s, (by + bh) * s,
                    border, 1.6f * s, 230);
                surf_.Line((bx + bw) * s, by * s, (bx + bw) * s, (by + bh) * s,
                    border, 1.6f * s, 230);
                surf_.FillRect(x * s, y * s, w * s, h * s, border, 16);
            }

            const float ind = (float)depth * 16.f;
            const float ax = x + ind + 3.f;
            if (hasChild) {
                surf_.FillRoundRect(ax * s, (y + 5.f) * s, 12.f * s, 12.f * s,
                    2.f * s, color::Control, 256);
                surf_.Line((ax + 3.f) * s, (y + 11.f) * s,
                    (ax + 9.f) * s, (y + 11.f) * s, color::TextMuted, 1.f * s, 220);
                if (!expanded)
                    surf_.Line((ax + 6.f) * s, (y + 8.f) * s,
                        (ax + 6.f) * s, (y + 14.f) * s, color::TextMuted, 1.f * s, 220);
                if (frameClick_ && !clickConsumed_ && Hit(ax - 2.f, y, 16.f, h)) {
                    clickConsumed_ = true;
                    out.toggled = true;
                }
            }
            const float iconX = ax + 17.f;
            const float iconSz = (std::min)(kProcIconSize, kRowH - 2.f);
            const float iconY = y + (kRowH - iconSz) * 0.5f;
            if (icon) {
                HDC dc = surf_.DC();
                if (!surf_.Suppressed() && dc) {
                    const int isz = (int)std::lround(iconSz * s);
                    const int ix = (int)std::lround(iconX * s);
                    const int iy = (int)std::lround(iconY * s);
                    const int cx0 = (int)surf_.ClipX0(), cy0 = (int)surf_.ClipY0();
                    const int cx1 = (int)surf_.ClipX1(), cy1 = (int)surf_.ClipY1();
                    if (isz > 0 && ix + isz > cx0 && ix < cx1 &&
                        iy + isz > cy0 && iy < cy1) {
                        SaveDC(dc);
                        IntersectClipRect(dc, cx0, cy0, cx1, cy1);
                        DrawIconEx(dc, ix, iy, icon, isz, isz, 0, nullptr, DI_NORMAL);
                        RestoreDC(dc, -1);
                    }
                }
            }
            else {
                surf_.FillRoundRect(iconX * s, iconY * s, iconSz * s, iconSz * s,
                    2.f * s, color::ControlHover, 256);
                surf_.FillRect(iconX * s, iconY * s, iconSz * s, 1.4f * s,
                    (state == 2) ? color::TerminatedRed
                    : (state == 1) ? color::NewGreen : color::Accent, 200);
            }

            const float tx = iconX + iconSz + 6.f;
            const float avail = (x + w) - tx - 4.f;
            const wchar_t* vals[7] = { name, pid, cpu, mem, company, desc, path };
            float cx = tx;
            for (int c = 0; c < 7; ++c) {
                const float cw = avail * kTreeColW[c];
                Text((cx + 6.f) * s, y * s, (cw - 8.f) * s, h * s,
                    (vals[c] && *vals[c]) ? vals[c] : L"-",
                    (c == 0) ? color::Text : color::TextMuted,
                    (c == 0) ? Font::UI : Font::UISmall,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                cx += cw;
            }

            if (frameRClick_ && !rclickConsumed_ && !menu_.open &&
                Hit(x, y, w, h)) {
                rclickConsumed_ = true;
                if (rightClickedRow) *rightClickedRow = rowIndex;
                ExampleOpenContextMenu(menuOwner);
            }
            if (frameClick_ && !clickConsumed_ && Hit(x, y, w, h)) {
                clickConsumed_ = true;
                out.selected = true;
            }
            return out;
        }

    private:
        struct SmoothScroll {
            const int* key = nullptr;
            float cur = 0.f;
            float target = 0.f;
        };

        SmoothScroll& FindSmooth(const int* key, float initial) noexcept {
            for (int i = 0; i < smoothCount_; ++i)
                if (smooth_[i].key == key) return smooth_[i];
            const int slot = (smoothCount_ < (int)smooth_.size()) ? smoothCount_++ : 0;
            smooth_[slot] = SmoothScroll{ key, initial, initial };
            return smooth_[slot];
        }

        void StartPageTransition() {
            menu_.open = false;
            menu_.hasHighlight = false;
            openDropdown_ = 0;
            overlay_.Reset();
            if (kPageFadeSec <= 0.f) {
                pageFadeActive_ = false;
                pageFadeT_ = 1.f;
                pageAlpha_ = 256.f;
                pageSlide_ = 0.f;
                animActive_ = false;
                RequestRedraw();
                return;
            }
            pageFadeActive_ = true;
            pageFadeT_ = 0.10f;
            const float u = 1.f - pageFadeT_;
            const float e = 1.f - u * u * u;
            pageAlpha_ = e * 256.f;
            pageSlide_ = (1.f - e) * kPageSlidePx;
            animActive_ = true;
            RequestRedraw();
        }

        void EnsurePrev(int rw, int rh) {
            if (rw <= 0 || rh <= 0) return;
            if (prevStride_ != rw || prevRows_ != rh) {
                prevStride_ = rw;
                prevRows_ = rh;
                prevPage_.assign((size_t)rw * rh, 0);
                prevValid_ = false;
            }
        }

        enum class Font {
            UI = 0, UIBold, UIBoldLarge, UISmall, Mono,
            IconRegular, IconFilled, Count
        };

        struct Region {
            float x = 0, y = 0, w = 0, h = 0;
            float cx = 0, cy = 0, rowH = 0.f, pad = kPanelPad;
            float scrollY = 0.f, viewY = 0.f, viewH = 0.f, animX = 0.f;
            bool clipped = false;
            bool inRow = false;
        };
        struct ItemRect {
            float x = 0, y = 0, w = 0, h = 0;
            int id = 0;
        };
        struct Overlay {
            bool active = false;
            int owner = 0;
            float x = 0, y = 0, w = 0;
            int count = 0;
            std::array<std::wstring, 16> items;
            int* target = nullptr;
            void Reset() { active = false; owner = 0; count = 0; target = nullptr; }
        };
        struct ContextMenu {
            bool open = false;
            bool justOpened = false;
            int owner = 0;
            float x = 0, y = 0, w = 0, h = 0;
            int count = 0;
            int clickedId = 0;
            bool hasHighlight = false;
            ItemRect highlight;
            std::array<ExampleMenuEntry, 16> entries;
        };

        void VisibleRange(float rowTop, float rh, int total,
            int& first, int& last) const noexcept {
            const float s = dpiScale_;
            const float cy0 = (float)surf_.ClipY0() / s;
            const float cy1 = (float)surf_.ClipY1() / s;
            first = (std::max)(0, (int)std::floor((cy0 - rowTop) / rh) - 1);
            last = (std::min)(total, (int)std::ceil((cy1 - rowTop) / rh) + 1);
            if (last < first) last = first;
        }

        static float MeasureLabel(const wchar_t* s) noexcept {
            float w = 0.f;
            for (; s && *s; ++s) w += (*s > 0x7F) ? 13.f : 7.f;
            return w;
        }

        bool Place(float& x, float& y, float& w, float& h, float wantW, float wantH,
            float gap = kGap) {
            Region& L = layout_.back();
            const float avail = L.w - 2.f * L.pad;
            if (avail <= 0.f) return false;
            if (wantW <= 0.f) wantW = avail;
            wantW = (std::min)(wantW, avail);
            if (L.inRow) {
                if (L.cx + wantW > L.x + L.w - L.pad + 0.5f) {
                    L.cy += L.rowH + kGap;
                    L.cx = L.x + L.pad;
                    L.rowH = 0.f;
                }
                x = L.cx + L.animX; y = L.cy - L.scrollY;
                L.cx += wantW + kGap;
                L.rowH = (std::max)(L.rowH, wantH);
            }
            else {
                x = L.x + L.pad + L.animX; y = L.cy - L.scrollY;
                L.cy += wantH + gap;
            }
            w = wantW; h = wantH;
            bool visible;
            if (L.clipped) {
                visible = (y + h > L.viewY && y < L.viewY + L.viewH);
            }
            else {
                visible = (y + h <= L.y + L.h - L.pad + 1.f);
            }
            if (!visible) return false;
            if (itemRectCount_ < (int)itemRects_.size()) {
                itemRects_[itemRectCount_] = ItemRect{ x, y, w, h, ++itemRectId_ };
                ++itemRectCount_;
            }
            return true;
        }
        bool RawHit(float x, float y, float w, float h) const noexcept {
            return mx_ >= x && mx_ < x + w && my_ >= y && my_ < y + h;
        }

        bool Hit(float x, float y, float w, float h) const noexcept {
            if (menuModal_) return false;
            if (blockHit_ && RawHit(blockX_, blockY_, blockW_, blockH_)) return false;
            return RawHit(x, y, w, h);
        }

        void Text(float x, float y, float w, float h, const wchar_t* s,
            U32 c, Font f, UINT flags) noexcept {
            if (surf_.Suppressed()) return;
            if (!s || !*s || !surf_.DC()) return;
            RECT r{ (int)x, (int)y, (int)(x + w), (int)(y + h) };
            const LONG cx0 = (LONG)surf_.ClipX0(), cy0 = (LONG)surf_.ClipY0();
            const LONG cx1 = (LONG)surf_.ClipX1(), cy1 = (LONG)surf_.ClipY1();
            r.left = (std::max)(r.left, cx0);
            r.top = (std::max)(r.top, cy0);
            r.right = (std::min)(r.right, cx1);
            r.bottom = (std::min)(r.bottom, cy1);
            if (r.right <= r.left || r.bottom <= r.top) return;
            HDC dc = surf_.DC();
            HFONT hf = fonts_[(int)f];
            if (hf != curFont_) { SelectObject(dc, hf); curFont_ = hf; }
            if (c != curColor_) { SetTextColor(dc, (COLORREF)CR(c)); curColor_ = c; }
            DrawTextW(dc, s, -1, &r, flags | DT_NOPREFIX);
        }

        void DrawOverlay() {
            if (!overlay_.active || overlay_.count == 0 || !overlay_.target) return;
            const float s = dpiScale_;
            const float ih = 28.f;
            const float H = (float)overlay_.count * ih + 8.f;
            const float X = overlay_.x, Y = overlay_.y, W = overlay_.w;
            surf_.RoundRect(X * s, Y * s, W * s, H * s, 6.f * s,
                color::Control, color::Stroke, 1.f * s);
            for (int i = 0; i < overlay_.count; ++i) {
                const float ry = Y + 4.f + (float)i * ih;
                if (RawHit(X, ry, W, ih)) {
                    surf_.FillRoundRect((X + 3.f) * s, ry * s, (W - 6.f) * s, ih * s,
                        4.f * s, color::Accent, 55);
                    if (frameClick_ && !clickConsumed_ && !menuModal_) {
                        clickConsumed_ = true;
                        *overlay_.target = i;
                        openDropdown_ = 0;
                        RequestRedraw();
                    }
                }
                Text((X + 12.f) * s, ry * s, (W - 24.f) * s, ih * s,
                    overlay_.items[i].c_str(),
                    (*overlay_.target == i) ? color::Accent : color::Text, Font::UI,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            if (frameClick_ && !clickConsumed_ && !menuModal_ &&
                !RawHit(X, Y, W, H)) {
                openDropdown_ = 0;
                RequestRedraw();
            }
        }

        void DrawContextMenu() {
            if (!menu_.open || menu_.count == 0) { menu_.hasHighlight = false; return; }
            const float s = dpiScale_;
            if (menu_.hasHighlight) {
                const ItemRect& r = menu_.highlight;
                surf_.FillRoundRect(r.x * s, r.y * s, r.w * s, r.h * s,
                    4.f * s, color::Accent, 30);
            }
            const float X = menu_.x, Y = menu_.y, W = menu_.w, H = menu_.h;
            const float ih = 30.f;
            surf_.FillRoundRect((X + 3.f) * s, (Y + 4.f) * s, W * s, H * s,
                9.f * s, 0x000000, 90);
            surf_.RoundRect(X * s, Y * s, W * s, H * s, 8.f * s,
                color::Panel, color::PanelStroke, 1.f * s);
            surf_.Line((X + 10.f) * s, (Y + 1.f) * s, (X + W - 10.f) * s, (Y + 1.f) * s,
                LerpColor(color::Text, color::Panel, 0.88f), 1.f * s, 60);
            bool esc = false;
            for (int k : frameKeys_) if (k == VK_ESCAPE) esc = true;
            float cy = Y + 6.f;
            for (int i = 0; i < menu_.count; ++i) {
                const ExampleMenuEntry& e = menu_.entries[i];
                if (!e.label) {
                    surf_.Line((X + 10.f) * s, (cy + 4.f) * s,
                        (X + W - 10.f) * s, (cy + 4.f) * s,
                        color::PanelStroke, 1.f * s, 210);
                    cy += 9.f;
                    continue;
                }
                const bool hov = e.enabled && RawHit(X + 4.f, cy, W - 8.f, ih);
                if (hov) {
                    surf_.FillRoundRect((X + 4.f) * s, (cy + 1.f) * s,
                        (W - 8.f) * s, (ih - 2.f) * s,
                        4.f * s, color::Accent, 38);
                    surf_.FillRect((X + 4.f) * s, (cy + 4.f) * s,
                        2.f * s, (ih - 8.f) * s, color::Accent, 220);
                }
                const float textX = e.checkable ? (X + 38.f) : (X + 14.f);
                if (e.checkable) {
                    const float ix = X + 14.f, iy = cy + (ih - 14.f) * 0.5f;
                    if (e.checked) {
                        surf_.FillRect(ix * s, iy * s, 14.f * s, 14.f * s, color::Accent, 55);
                        surf_.Line((ix + 3.f) * s, (iy + 7.f) * s,
                            (ix + 6.f) * s, (iy + 10.f) * s, color::Accent, 1.8f * s);
                        surf_.Line((ix + 6.f) * s, (iy + 10.f) * s,
                            (ix + 11.f) * s, (iy + 4.f) * s, color::Accent, 1.8f * s);
                    }
                    else {
                        surf_.FillRect((ix + 2.f) * s, (iy + 2.f) * s,
                            10.f * s, 10.f * s, color::TextDim, 90);
                    }
                }
                const float shortW = e.shortcut ? MeasureLabel(e.shortcut) : 0.f;
                Text(textX * s, cy * s,
                    (X + W - textX - 14.f - shortW - 10.f) * s, ih * s, e.label,
                    e.enabled ? color::Text : color::TextDim, Font::UI,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                if (e.shortcut)
                    Text((X + W - 14.f - shortW) * s, cy * s, shortW * s, ih * s,
                        e.shortcut, color::TextDim, Font::UISmall,
                        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                if (hov && frameClick_ && !clickConsumed_) {
                    clickConsumed_ = true;
                    menu_.clickedId = e.id;
                    menu_.open = false;
                    RequestRedraw();
                }
                cy += ih;
            }
            if (esc || (!menu_.justOpened && menu_.open &&
                (frameClick_ || frameRClick_) && !RawHit(X, Y, W, H))) {
                menu_.open = false;
                RequestRedraw();
            }
        }

        void DestroyFonts() {
            for (auto& f : fonts_) { if (f) { DeleteObject(f); f = nullptr; } }
            curFont_ = nullptr;
        }
        void BuildFonts() {
            DestroyFonts();
            ExampleIconFonts::Ensure();
            const float s = dpiScale_;
            const float f = (std::max)(0.5f, kFontScale);
            auto mk = [s, f](int px, int weight, const wchar_t* face) -> HFONT {
                return CreateFontW(-(int)std::lround(px * f * s), 0, 0, 0, weight,
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
                };
            const wchar_t* face = ExampleIconFonts::UiFace();
            fonts_[(int)Font::UI] = mk(14, 400, face);
            fonts_[(int)Font::UIBold] = mk(14, 600, face);
            fonts_[(int)Font::UIBoldLarge] = mk(19, 600, face);
            fonts_[(int)Font::UISmall] = mk(12, 400, face);
            fonts_[(int)Font::Mono] = mk(13, 400, L"Consolas");
            fonts_[(int)Font::IconRegular] = mk(20, 400, ExampleIconFonts::RegularFace());
            fonts_[(int)Font::IconFilled] = mk(20, 400, ExampleIconFonts::FilledFace());
            builtFontScale_ = kFontScale;
            builtDpi_ = s;
        }

        static LRESULT CALLBACK WndProcThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
            ExampleGUI* self = nullptr;
            if (m == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
                self = static_cast<ExampleGUI*>(cs->lpCreateParams);
                SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else {
                self = reinterpret_cast<ExampleGUI*>(GetWindowLongPtrW(h, GWLP_USERDATA));
            }
            return self ? self->WndProc(h, m, w, l) : DefWindowProcW(h, m, w, l);
        }

        LRESULT WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
            switch (m) {
            case WM_SIZE: {
                const int cw = (int)(short)LOWORD(l), ch = (int)(short)HIWORD(l);
                surf_.Resize(cw, ch);
                gdiDirty_ = true;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_DPICHANGED: {
                dpiScale_ = (float)(UINT)LOWORD(w) / 96.f;
                if (dpiScale_ < 1.f) dpiScale_ = 1.f;
                BuildFonts();
                gdiDirty_ = true;
                auto* rc = reinterpret_cast<RECT*>(l);
                SetWindowPos(h, nullptr, rc->left, rc->top, rc->right - rc->left,
                    rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_MOUSEMOVE: {
                mx_ = (float)(int)(short)LOWORD(l);
                my_ = (float)(int)(short)HIWORD(l);
                mx_ /= dpiScale_; my_ /= dpiScale_;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                mx_ = (float)(int)(short)LOWORD(l) / dpiScale_;
                my_ = (float)(int)(short)HIWORD(l) / dpiScale_;
                mouseDown_ = true; clickPending_ = true;
                SetCapture(h);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_LBUTTONUP: {
                mouseDown_ = false;
                ReleaseCapture();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_RBUTTONUP: {
                mx_ = (float)(int)(short)LOWORD(l) / dpiScale_;
                my_ = (float)(int)(short)HIWORD(l) / dpiScale_;
                rclickPending_ = true;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_MOUSEWHEEL: {
                POINT p{ (int)(short)LOWORD(l), (int)(short)HIWORD(l) };
                ScreenToClient(h, &p);
                mx_ = (float)p.x / dpiScale_; my_ = (float)p.y / dpiScale_;
                wheelAccum_ += (int)(short)HIWORD(w);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_CHAR:
                if (w >= 0x20 || w == VK_BACK) charQueue_.push_back((wchar_t)w);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            case WM_KEYDOWN:
                keyQueue_.push_back((int)w);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            case WM_TIMER:
                if (w == 2 && topmost_) ReassertTopmost();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(h, &ps);
                if (surf_.Width() > 0 && surf_.Height() > 0) {
                    Begin();
                    if (onDraw_) onDraw_(*this);
                    End();
                    BitBlt(dc, 0, 0, surf_.Width(), surf_.Height(), surf_.DC(), 0, 0, SRCCOPY);
                }
                EndPaint(h, &ps);
                return 0;
            }
            case WM_GETMINMAXINFO: {
                auto* mm = reinterpret_cast<MINMAXINFO*>(l);
                mm->ptMinTrackSize.x = (LONG)(800 * dpiScale_);
                mm->ptMinTrackSize.y = (LONG)(560 * dpiScale_);
                return 0;
            }
            case WM_DESTROY:
                if (timerId_) { KillTimer(h, timerId_); timerId_ = 0; }
                if (animTimerId_) { KillTimer(h, animTimerId_); animTimerId_ = 0; }
                if (zTimerId_) { KillTimer(h, zTimerId_); zTimerId_ = 0; }
                PostQuitMessage(0);
                return 0;
            default: break;
            }
            return DefWindowProcW(h, m, w, l);
        }

        struct MaskRegion {
            int id = 0;
            std::wstring name;
            bool isStatic = false;
            float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
            int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
            unsigned long long sig = 0;
            bool redrawn = false;
            int redraws = 0, skips = 0;
        };

        MaskRegion& MaskById(int id) {
            for (auto& r : masks_) if (r.id == id) return r;
            masks_.push_back(MaskRegion{});
            masks_.back().id = id;
            std::sort(masks_.begin(), masks_.end(),
                [](const MaskRegion& a, const MaskRegion& b) { return a.id < b.id; });
            for (auto& r : masks_) if (r.id == id) return r;
            return masks_.back();
        }

        void DrawMaskOverlay() {
            if (!maskOverlay_ || masks_.empty()) return;
            if (menu_.open || overlay_.active) return;
            surf_.SetSuppress(false);
            for (const auto& r : masks_) {
                if (r.px1 <= r.px0 || r.py1 <= r.py0) continue;
                const float x = (float)r.px0, y = (float)r.py0;
                const float w = (float)(r.px1 - r.px0);
                const float h = (float)(r.py1 - r.py0);
                const bool masked = r.isStatic || masksEnabled_;
                if (masked)
                    surf_.FillRect(x, y, w, h, 0x001A2430u, 238);
                const U32 edge = masked ? 0x0060CDFFu : 0x006A9BFFu;
                surf_.Line(x, y, x + w, y, edge, 1.f, 220);
                surf_.Line(x, y + h - 1.f, x + w, y + h - 1.f, edge, 1.f, 220);
                surf_.Line(x, y, x, y + h, edge, 1.f, 220);
                surf_.Line(x + w - 1.f, y, x + w - 1.f, y + h, edge, 1.f, 220);
                wchar_t t[320];
                std::swprintf(t, 320, L"#%d %s  %s  %s  draw=%d skip=%d",
                    r.id, r.name.c_str(),
                    r.isStatic ? L"STATIC" : L"DYNAMIC",
                    masked ? L"跳过绘制" : (r.redrawn ? L"DRAW" : L"SKIP"),
                    r.redraws, r.skips);
                surf_.FillRect(x + 2.f, y + 2.f,
                    (std::min)(w - 4.f, 380.f), 19.f, 0x00101820u, 240);
                Text(x + 5.f, y + 2.f,
                    (std::min)(w - 10.f, 450.f), 19.f, t,
                    0x00FFFFFFu, Font::UI, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        HINSTANCE inst_ = nullptr;
        HWND hwnd_ = nullptr;
        Surface surf_;
        DrawFn onDraw_;
        HFONT fonts_[(int)Font::Count]{};
        HFONT curFont_ = nullptr;
        U32 curColor_ = 0xFFFFFFFFu;
        bool gdiDirty_ = true;
        float dpiScale_ = 1.f;
        float builtFontScale_ = 1.f;
        float builtDpi_ = 1.f;
        UINT_PTR timerId_ = 0;
        UINT_PTR animTimerId_ = 0;
        UINT_PTR zTimerId_ = 0;
        bool topmost_ = false;
        std::vector<MaskRegion> masks_;
        bool masksEnabled_ = false;
        bool maskOverlay_ = false;
        int activeMask_ = -1;
        int frameSkipped_ = 0, frameRedrawn_ = 0;

        std::array<SmoothScroll, 16> smooth_{};
        int smoothCount_ = 0;

        std::vector<U32> prevPage_;
        int prevStride_ = 0, prevRows_ = 0;
        bool prevValid_ = false;
        int pagePxX0_ = 0, pagePxY0_ = 0, pagePxX1_ = 0, pagePxY1_ = 0;
        bool pageFadeActive_ = false;
        float pageFadeT_ = 0.f;
        float pageAlpha_ = 256.f;
        float pageSlide_ = 0.f;
        float frameDt_ = 0.f;
        bool animActive_ = false;
        std::chrono::steady_clock::time_point lastFrame_{};

        std::vector<Region> layout_;
        Overlay overlay_;
        ContextMenu menu_;
        std::vector<ItemRect> itemRects_ = std::vector<ItemRect>(1024);
        int itemRectCount_ = 0;
        int itemRectId_ = 0;
        int pageItemStart_ = 0;
        float pageViewY_ = 0.f;
        float* pageMeasuredPtr_ = nullptr;
        bool sbDrag_ = false;
        float sbDragOffset_ = 0.f;
        int openDropdown_ = 0;
        bool menuModal_ = false;
        bool blockHit_ = false;
        float blockX_ = 0, blockY_ = 0, blockW_ = 0, blockH_ = 0;
        int idCounter_ = 0;
        int activeId_ = 0;
        int sliderDragId_ = 0;
        int sliderEditId_ = 0;
        float sliderEditOrig_ = 0.f;
        bool sliderEditFresh_ = false;
        std::wstring sliderEditBuf_;
        int frameCount_ = 0;
        bool connected_ = false;

        float mx_ = -1.f, my_ = -1.f;
        bool mouseDown_ = false;
        bool clickPending_ = false;
        bool rclickPending_ = false;
        int wheelAccum_ = 0;
        std::vector<wchar_t> charQueue_, frameChars_;
        std::vector<int> keyQueue_, frameKeys_;
        bool frameClick_ = false;
        bool frameRClick_ = false;
        bool clickConsumed_ = false;
        bool rclickConsumed_ = false;
        int frameWheel_ = 0;
    };

    class ExampleConfirmWindow {
    public:
        enum { ZTopmost = 0, ZAboveMain = 1, ZNormal = 2 };

        ExampleConfirmWindow() { s_all.push_back(this); }
        ~ExampleConfirmWindow() {
            for (auto it = s_all.begin(); it != s_all.end(); ++it) {
                if (*it == this) { s_all.erase(it); break; }
            }
            DestroyFonts();
            if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
        }
        ExampleConfirmWindow(const ExampleConfirmWindow&) = delete;
        ExampleConfirmWindow& operator=(const ExampleConfirmWindow&) = delete;

        bool Create(HINSTANCE inst, HWND owner) {
            inst_ = inst;
            owner_ = owner;
            dpiScale_ = (float)WindowDpi(owner ? owner : nullptr) / 96.f;
            if (dpiScale_ < 1.f) dpiScale_ = 1.f;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = &ExampleConfirmWindow::Thunk;
            wc.hInstance = inst_;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = L"ExampleFluentGUI_Confirm";
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

            const DWORD ex = WS_EX_TOOLWINDOW;
            const int w = (int)std::lround(kW * dpiScale_);
            const int h = (int)std::lround(kH * dpiScale_);
            hwnd_ = CreateWindowExW(ex, wc.lpszClassName, L"", WS_POPUP,
                CW_USEDEFAULT, CW_USEDEFAULT, w, h, owner_, nullptr, inst_, this);
            if (!hwnd_) return false;
            BuildFonts();
            surf_.Resize(w, h);
            ApplyDarkTitleBar(hwnd_);
            return true;
        }

        void Show(const wchar_t* text) {
            if (!hwnd_) return;
            text_ = text ? text : L"";
            result_ = 0;
            hover_ = 0;
            dragging_ = false;
            visible_ = true;
            CenterOnOwner();
            ShowWindow(hwnd_, SW_SHOWNORMAL);
            ApplyZ();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        void Hide() noexcept {
            visible_ = false;
            if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
        }
        bool Visible() const noexcept { return visible_; }
        int  Poll() noexcept { const int r = result_; result_ = 0; return r; }

        void SetZMode(int m) noexcept {
            zMode_ = (std::max)(0, (std::min)(2, m));
            ApplyZ();
        }
        int  ZModeValue() const noexcept { return zMode_; }
        HWND Handle() const noexcept { return hwnd_; }

        void RequestRedraw() const noexcept {
            if (hwnd_ && visible_) InvalidateRect(hwnd_, nullptr, FALSE);
        }
        static void BroadcastRedraw() noexcept {
            for (auto* p : s_all) if (p) p->RequestRedraw();
        }

    private:
        static constexpr float kW = 420.f;
        static constexpr float kH = 180.f;
        inline static std::vector<ExampleConfirmWindow*> s_all{};

        void BuildFonts() {
            DestroyFonts();
            ExampleIconFonts::Ensure();
            const float s = dpiScale_;
            auto mk = [s](int px, int weight, const wchar_t* face) {
                return CreateFontW((int)std::lround(px * s), 0, 0, 0, weight,
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, face);
                };
            const wchar_t* face = ExampleIconFonts::UiFace();
            fonts_[0] = mk(17, 400, face);
            fonts_[1] = mk(18, 600, face);
            fonts_[2] = mk(14, 400, face);
        }
        void DestroyFonts() noexcept {
            for (auto& f : fonts_) { if (f) { DeleteObject(f); f = nullptr; } }
        }

        void CenterOnOwner() noexcept {
            if (!hwnd_) return;
            const int w = (int)std::lround(kW * dpiScale_);
            const int h = (int)std::lround(kH * dpiScale_);
            RECT r{};
            if (owner_ && GetWindowRect(owner_, &r)) {
                SetWindowPos(hwnd_, nullptr,
                    r.left + ((r.right - r.left) - w) / 2,
                    r.top + ((r.bottom - r.top) - h) / 2,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            else {
                const int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
                SetWindowPos(hwnd_, nullptr, (sw - w) / 2, (sh - h) / 2,
                    0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        void ApplyZ() noexcept {
            if (!hwnd_ || !visible_) return;
            const UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
            if (zMode_ == ZTopmost) SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, f);
            else if (zMode_ == ZAboveMain && owner_)
                SetWindowPos(hwnd_, owner_, 0, 0, 0, 0, f);
            else SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0, f);
        }

        void Text(float x, float y, float w, float h, const wchar_t* s,
            U32 c, HFONT f, UINT flags) noexcept {
            if (!s || !*s || !surf_.DC()) return;
            RECT r{ (int)x, (int)y, (int)(x + w), (int)(y + h) };
            HDC dc = surf_.DC();
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, (COLORREF)c);
            HGDIOBJ old = SelectObject(dc, f);
            DrawTextW(dc, s, -1, &r, flags);
            SelectObject(dc, old);
        }

        void Paint() noexcept {
            if (!surf_.Width() || !surf_.Height()) return;
            const float s = dpiScale_;
            auto P = [s](float v) noexcept { return v * s; };
            surf_.SetSuppress(false);
            surf_.ResetClip();
            surf_.Clear(color::BgTop);

            const float cx = P(10.f), cy = P(10.f);
            const float cw = P(kW - 20.f), ch = P(kH - 20.f);
            surf_.FillRoundRect(cx + P(4.f), cy + P(4.f), cw, ch, P(9.f),
                ExampleScaleColor(color::BgTop, 0.70f, 0), 256);
            surf_.FillRoundRect(cx, cy, cw, ch, P(9.f), color::PanelStroke);
            surf_.FillRoundRect(cx + P(1.f), cy + P(1.f), cw - P(2.f), ch - P(2.f),
                P(8.f), color::Panel);

            const float closeX = kW - 34.f, closeY = 14.f, closeW = 22.f;
            const float sep1 = 48.f;
            const float sep2 = kH - 54.f;
            const float by = kH - 44.f, bh = 30.f;
            const float bw1 = 112.f, bw2 = 76.f;
            const float bx2 = kW - 22.f - bw2;
            const float bx1 = bx2 - 10.f - bw1;

            surf_.Circle(P(28.f), P(29.f), P(4.5f), color::Warn);
            Text(P(42.f), P(16.f), P(closeX - 52.f), P(26.f), L"待确认",
                color::Text, fonts_[1], DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if (hover_ == 3)
                surf_.FillRoundRect(P(closeX), P(closeY), P(closeW), P(closeW),
                    P(4.f), color::ControlHover);
            Text(P(closeX), P(closeY), P(closeW), P(closeW), L"\u2715",
                color::TextMuted, fonts_[2], DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            surf_.Line(P(20.f), P(sep1), P(kW - 20.f), P(sep1),
                color::PanelStroke, 1.f, 210);
            Text(P(20.f), P(sep1 + 7.f), P(kW - 40.f), P(sep2 - sep1 - 12.f),
                text_.c_str(), color::Text, fonts_[0],
                DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
            surf_.Line(P(20.f), P(sep2), P(kW - 20.f), P(sep2),
                color::PanelStroke, 1.f, 210);

            const U32 f1 = (hover_ == 1) ? color::AccentPress : color::Accent;
            const U32 f2 = (hover_ == 2) ? color::ControlHover : color::Control;
            surf_.FillRoundRect(P(bx1), P(by), P(bw1), P(bh), P(4.f), f1);
            surf_.FillRoundRect(P(bx2), P(by), P(bw2), P(bh), P(4.f), f2);
            Text(P(bx1), P(by), P(bw1), P(bh), L"确认执行",
                color::OnAccent, fonts_[1], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            Text(P(bx2), P(by), P(bw2), P(bh), L"取消",
                color::Text, fonts_[1], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        int HitTest(float x, float y) const noexcept {
            const float closeX = kW - 34.f;
            if (x >= closeX && x <= closeX + 22.f &&
                y >= 14.f && y <= 36.f) return 3;
            const float by = kH - 44.f;
            const float bx2 = kW - 22.f - 76.f;
            const float bx1 = bx2 - 10.f - 112.f;
            if (y >= by && y <= by + 30.f) {
                if (x >= bx1 && x <= bx1 + 112.f) return 1;
                if (x >= bx2 && x <= bx2 + 76.f) return 2;
            }
            return 0;
        }
        static LRESULT CALLBACK Thunk(HWND h, UINT m, WPARAM w, LPARAM l) {
            ExampleConfirmWindow* self = nullptr;
            if (m == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
                self = static_cast<ExampleConfirmWindow*>(cs->lpCreateParams);
                SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else {
                self = reinterpret_cast<ExampleConfirmWindow*>(
                    GetWindowLongPtrW(h, GWLP_USERDATA));
            }
            return self ? self->WndProc(h, m, w, l) : DefWindowProcW(h, m, w, l);
        }

        LRESULT WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
            switch (m) {
            case WM_SIZE: {
                const int cw = (int)(short)LOWORD(l), ch = (int)(short)HIWORD(l);
                surf_.Resize(cw, ch);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            case WM_MOUSEMOVE: {
                mx_ = (float)(int)(short)LOWORD(l) / dpiScale_;
                my_ = (float)(int)(short)HIWORD(l) / dpiScale_;
                const int hv = HitTest(mx_, my_);
                if (hv != hover_) { hover_ = hv; InvalidateRect(h, nullptr, FALSE); }
                if (dragging_) {
                    POINT p{};
                    if (GetCursorPos(&p)) {
                        SetWindowPos(h, nullptr,
                            winX_ + (p.x - dragX_), winY_ + (p.y - dragY_),
                            0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
                return 0;
            }
            case WM_LBUTTONDOWN: {
                mx_ = (float)(int)(short)LOWORD(l) / dpiScale_;
                my_ = (float)(int)(short)HIWORD(l) / dpiScale_;
                if (HitTest(mx_, my_) == 0 && my_ >= 10.f && my_ <= 46.f) {
                    POINT p{}; RECT r{};
                    if (GetCursorPos(&p) && GetWindowRect(h, &r)) {
                        dragX_ = p.x; dragY_ = p.y;
                        winX_ = r.left; winY_ = r.top;
                        dragging_ = true;
                        SetCapture(h);
                    }
                }
                return 0;
            }
            case WM_LBUTTONUP: {
                if (dragging_) {
                    dragging_ = false;
                    ReleaseCapture();
                    return 0;
                }
                mx_ = (float)(int)(short)LOWORD(l) / dpiScale_;
                my_ = (float)(int)(short)HIWORD(l) / dpiScale_;
                const int hit = HitTest(mx_, my_);
                if (hit == 1) { result_ = 1; Hide(); }
                else if (hit == 2 || hit == 3) { result_ = 2; Hide(); }
                return 0;
            }
            case WM_KEYDOWN:
                if (w == VK_ESCAPE) { result_ = 2; Hide(); }
                else if (w == VK_RETURN) { result_ = 1; Hide(); }
                return 0;
            case WM_CLOSE:
                result_ = 2; Hide();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps{};
                HDC dc = BeginPaint(h, &ps);
                if (surf_.Width() > 0 && surf_.Height() > 0) {
                    Paint();
                    BitBlt(dc, 0, 0, surf_.Width(), surf_.Height(),
                        surf_.DC(), 0, 0, SRCCOPY);
                }
                EndPaint(h, &ps);
                return 0;
            }
            case WM_TIMER:
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            case WM_DESTROY:
                return 0;
            default: break;
            }
            return DefWindowProcW(h, m, w, l);
        }

        HINSTANCE inst_ = nullptr;
        HWND hwnd_ = nullptr;
        HWND owner_ = nullptr;
        Surface surf_;
        HFONT fonts_[3]{};
        float dpiScale_ = 1.f;
        float mx_ = -1.f, my_ = -1.f;
        int hover_ = 0;
        int result_ = 0;
        int zMode_ = ZTopmost;
        bool visible_ = false;
        bool dragging_ = false;
        int dragX_ = 0, dragY_ = 0, winX_ = 0, winY_ = 0;
        std::wstring text_;
    };

}

#endif

