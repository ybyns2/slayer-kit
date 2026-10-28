#pragma once
#include <d2d1_1.h>
#include <dwrite_1.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <map>
#include <string>

#include "core/Util.h"

namespace sk::ui {

using Microsoft::WRL::ComPtr;

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    float r() const { return x + w; }
    float b() const { return y + h; }
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
    Rect inset(float dx, float dy) const { return {x + dx, y + dy, w - 2 * dx, h - 2 * dy}; }
    Rect offset(float dx, float dy) const { return {x + dx, y + dy, w, h}; }
    D2D1_RECT_F d2d() const { return D2D1::RectF(x, y, x + w, y + h); }
};

struct Color {
    float r = 0, g = 0, b = 0, a = 1;
    Color alpha(float na) const { return {r, g, b, a * na}; }
    Color mix(const Color& o, float t) const { return {r + (o.r - r) * t, g + (o.g - g) * t, b + (o.b - b) * t, a + (o.a - a) * t}; }
    Color lighten(float t) const { return mix({1, 1, 1, a}, t); }
    Color darken(float t) const { return mix({0, 0, 0, a}, t); }
    D2D1_COLOR_F d2d() const { return {r, g, b, a}; }
    static Color Hex(unsigned v, float a = 1) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, a}; }
};

enum class Font { Display, Title, Heading, Body, BodyBold, Small, SmallBold, Caption, Code, Icon };
enum class Align { Left, Center, Right };

class Gfx {
public:
    bool Init(HWND hwnd);
    void Resize(UINT w, UINT h);
    void SetDpi(float dpi);
    float Dpi() const { return dpi_; }
    float Width() const { return width_; }
    float Height() const { return height_; }

    bool Begin();
    bool End();
    ID2D1DeviceContext* Ctx() { return ctx_.Get(); }

    void Clear(const Color& c);
    void Fill(const Rect& r, const Color& c, float radius = 0);
    void Stroke(const Rect& r, const Color& c, float radius = 0, float width = 1);
    void FillGradient(const Rect& r, const Color& a, const Color& b, bool vertical, float radius = 0);
    void FillRadial(float cx, float cy, float rx, float ry, const Color& inner, const Color& outer);
    void Line(float x1, float y1, float x2, float y2, const Color& c, float width = 1);
    void Circle(float cx, float cy, float radius, const Color& c);
    void Ring(float cx, float cy, float radius, const Color& c, float width);
    void Arc(float cx, float cy, float radius, float startDeg, float sweepDeg, const Color& c, float width);
    void Glow(const Rect& r, const Color& c, float radius, float spread);
    void Polygon(const std::initializer_list<D2D1_POINT_2F>& pts, const Color& c);

    void Text(const std::wstring& s, const Rect& r, Font f, const Color& c, Align a = Align::Left, bool vcenter = true,
              bool wrap = false);
    float MeasureWidth(const std::wstring& s, Font f);
    float MeasureHeight(const std::wstring& s, Font f, float width);

    ID2D1Bitmap* Image(const fs::path& p);
    ID2D1Bitmap* ImageFromResource(int id);
    void DrawImage(ID2D1Bitmap* bmp, const Rect& r, float opacity = 1, bool cover = false, float radius = 0);

    void PushClip(const Rect& r, float radius = 0);
    void PopClip();
    void PushOpacity(float o);
    void PopOpacity();
    void SetTransform(float dx, float dy, float scale = 1, float cx = 0, float cy = 0);
    void ResetTransform();

private:
    ID2D1SolidColorBrush* Brush(const Color& c);
    IDWriteTextFormat* Format(Font f);
    ComPtr<IDWriteTextLayout> Layout(const std::wstring& s, Font f, float w, float h, bool wrap);

    HWND hwnd_ = nullptr;
    float dpi_ = 96, width_ = 0, height_ = 0;
    ComPtr<ID2D1Factory1> factory_;
    ComPtr<IDWriteFactory1> dwrite_;
    ComPtr<IWICImagingFactory> wic_;
    ComPtr<ID2D1HwndRenderTarget> target_;
    ComPtr<ID2D1DeviceContext> ctx_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    std::map<Font, ComPtr<IDWriteTextFormat>> formats_;
    std::map<std::wstring, ComPtr<ID2D1Bitmap>> images_;
    std::vector<bool> clipIsLayer_;
    int opacityDepth_ = 0;
};

}
