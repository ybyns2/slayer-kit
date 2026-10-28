#include "ui/Gfx.h"

#include <cmath>

namespace sk::ui {

namespace {

struct FontSpec {
    const wchar_t* family;
    const wchar_t* fallback;
    float size;
    DWRITE_FONT_WEIGHT weight;
    DWRITE_FONT_STRETCH stretch;
};

FontSpec Spec(Font f) {
    switch (f) {
    case Font::Display: return {L"Bahnschrift", L"Segoe UI", 40, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_SEMI_CONDENSED};
    case Font::Title: return {L"Bahnschrift", L"Segoe UI", 27, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL};
    case Font::Heading: return {L"Bahnschrift", L"Segoe UI", 17, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL};
    case Font::Body: return {L"Segoe UI Variable Text", L"Segoe UI", 14, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL};
    case Font::BodyBold: return {L"Segoe UI Variable Text", L"Segoe UI", 14, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL};
    case Font::Small: return {L"Segoe UI Variable Text", L"Segoe UI", 12.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL};
    case Font::SmallBold: return {L"Segoe UI Variable Text", L"Segoe UI", 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL};
    case Font::Caption: return {L"Bahnschrift", L"Segoe UI", 11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL};
    case Font::Code: return {L"Cascadia Mono", L"Consolas", 30, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL};
    case Font::Icon: return {L"Segoe Fluent Icons", L"Segoe MDL2 Assets", 15, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL};
    }
    return {L"Segoe UI", L"Segoe UI", 14, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL};
}

}

bool Gfx::Init(HWND hwnd) {
    hwnd_ = hwnd;
    D2D1_FACTORY_OPTIONS opts{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts,
                                 reinterpret_cast<void**>(factory_.GetAddressOf()))))
        return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1),
                                   reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf()))))
        return false;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_));
    dpi_ = (float)GetDpiForWindow(hwnd);
    return true;
}

void Gfx::SetDpi(float dpi) {
    dpi_ = dpi;
    if (target_) target_->SetDpi(dpi, dpi);
    RECT rc;
    GetClientRect(hwnd_, &rc);
    width_ = (rc.right - rc.left) * 96.f / dpi_;
    height_ = (rc.bottom - rc.top) * 96.f / dpi_;
}

void Gfx::Resize(UINT w, UINT h) {
    width_ = w * 96.f / dpi_;
    height_ = h * 96.f / dpi_;
    if (target_) target_->Resize(D2D1::SizeU(w, h));
}

bool Gfx::Begin() {
    if (!target_) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                  D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                                  dpi_, dpi_);
        auto hprops = D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top),
                                                       D2D1_PRESENT_OPTIONS_NONE);
        if (FAILED(factory_->CreateHwndRenderTarget(props, hprops, &target_))) return false;
        target_.As(&ctx_);
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        width_ = (rc.right - rc.left) * 96.f / dpi_;
        height_ = (rc.bottom - rc.top) * 96.f / dpi_;
        brush_.Reset();
        images_.clear();
    }
    target_->BeginDraw();
    target_->SetTransform(D2D1::Matrix3x2F::Identity());
    return true;
}

bool Gfx::End() {
    HRESULT hr = target_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        target_.Reset();
        ctx_.Reset();
        brush_.Reset();
        images_.clear();
        return false;
    }
    return SUCCEEDED(hr);
}

ID2D1SolidColorBrush* Gfx::Brush(const Color& c) {
    if (!brush_) target_->CreateSolidColorBrush(c.d2d(), &brush_);
    else brush_->SetColor(c.d2d());
    return brush_.Get();
}

void Gfx::Clear(const Color& c) { target_->Clear(c.d2d()); }

void Gfx::Fill(const Rect& r, const Color& c, float radius) {
    if (radius > 0) target_->FillRoundedRectangle(D2D1::RoundedRect(r.d2d(), radius, radius), Brush(c));
    else target_->FillRectangle(r.d2d(), Brush(c));
}

void Gfx::Stroke(const Rect& r, const Color& c, float radius, float width) {
    Rect s = r.inset(width / 2);
    if (radius > 0) target_->DrawRoundedRectangle(D2D1::RoundedRect(s.d2d(), radius, radius), Brush(c), width);
    else target_->DrawRectangle(s.d2d(), Brush(c), width);
}

void Gfx::FillGradient(const Rect& r, const Color& a, const Color& b, bool vertical, float radius) {
    D2D1_GRADIENT_STOP stops[2] = {{0, a.d2d()}, {1, b.d2d()}};
    ComPtr<ID2D1GradientStopCollection> coll;
    target_->CreateGradientStopCollection(stops, 2, &coll);
    ComPtr<ID2D1LinearGradientBrush> br;
    auto end = vertical ? D2D1::Point2F(r.x, r.b()) : D2D1::Point2F(r.r(), r.y);
    target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(r.x, r.y), end), coll.Get(), &br);
    if (radius > 0) target_->FillRoundedRectangle(D2D1::RoundedRect(r.d2d(), radius, radius), br.Get());
    else target_->FillRectangle(r.d2d(), br.Get());
}

void Gfx::FillRadial(float cx, float cy, float rx, float ry, const Color& inner, const Color& outer) {
    D2D1_GRADIENT_STOP stops[2] = {{0, inner.d2d()}, {1, outer.d2d()}};
    ComPtr<ID2D1GradientStopCollection> coll;
    target_->CreateGradientStopCollection(stops, 2, &coll);
    ComPtr<ID2D1RadialGradientBrush> br;
    target_->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0), rx, ry), coll.Get(), &br);
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rx, ry), br.Get());
}

void Gfx::Line(float x1, float y1, float x2, float y2, const Color& c, float width) {
    target_->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), Brush(c), width);
}

void Gfx::Circle(float cx, float cy, float radius, const Color& c) {
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), Brush(c));
}

void Gfx::Ring(float cx, float cy, float radius, const Color& c, float width) {
    target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), Brush(c), width);
}

void Gfx::Arc(float cx, float cy, float radius, float startDeg, float sweepDeg, const Color& c, float width) {
    if (std::fabs(sweepDeg) < 0.5f) return;
    ComPtr<ID2D1PathGeometry> geo;
    factory_->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    auto pt = [&](float deg) {
        float rad = deg * 3.14159265f / 180.f;
        return D2D1::Point2F(cx + radius * std::cos(rad), cy + radius * std::sin(rad));
    };
    sink->BeginFigure(pt(startDeg), D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddArc(D2D1::ArcSegment(pt(startDeg + sweepDeg), D2D1::SizeF(radius, radius), 0,
                                  sweepDeg > 0 ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                  std::fabs(sweepDeg) > 180 ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();
    ComPtr<ID2D1StrokeStyle> style;
    factory_->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &style);
    target_->DrawGeometry(geo.Get(), Brush(c), width, style.Get());
}

void Gfx::Glow(const Rect& r, const Color& c, float radius, float spread) {
    const int steps = 8;
    for (int i = steps; i >= 1; --i) {
        float t = (float)i / steps;
        float d = spread * t;
        Fill({r.x - d, r.y - d, r.w + 2 * d, r.h + 2 * d}, c.alpha((1 - t) * (1 - t) * 0.22f), radius + d);
    }
}

void Gfx::Polygon(const std::initializer_list<D2D1_POINT_2F>& pts, const Color& c) {
    ComPtr<ID2D1PathGeometry> geo;
    factory_->CreatePathGeometry(&geo);
    ComPtr<ID2D1GeometrySink> sink;
    geo->Open(&sink);
    auto it = pts.begin();
    sink->BeginFigure(*it, D2D1_FIGURE_BEGIN_FILLED);
    for (++it; it != pts.end(); ++it) sink->AddLine(*it);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    target_->FillGeometry(geo.Get(), Brush(c));
}

IDWriteTextFormat* Gfx::Format(Font f) {
    auto it = formats_.find(f);
    if (it != formats_.end()) return it->second.Get();
    FontSpec s = Spec(f);
    ComPtr<IDWriteTextFormat> fmt;
    ComPtr<IDWriteFontCollection> sys;
    dwrite_->GetSystemFontCollection(&sys);
    UINT32 idx;
    BOOL exists = FALSE;
    sys->FindFamilyName(s.family, &idx, &exists);
    dwrite_->CreateTextFormat(exists ? s.family : s.fallback, nullptr, s.weight, DWRITE_FONT_STYLE_NORMAL, s.stretch,
                              s.size, L"fr-fr", &fmt);
    formats_[f] = fmt;
    return fmt.Get();
}

ComPtr<IDWriteTextLayout> Gfx::Layout(const std::wstring& s, Font f, float w, float h, bool wrap) {
    ComPtr<IDWriteTextLayout> layout;
    dwrite_->CreateTextLayout(s.c_str(), (UINT32)s.size(), Format(f), std::max(w, 1.f), std::max(h, 1.f), &layout);
    if (layout) {
        layout->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
        if (!wrap) {
            DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            ComPtr<IDWriteInlineObject> ellipsis;
            dwrite_->CreateEllipsisTrimmingSign(Format(f), &ellipsis);
            layout->SetTrimming(&trim, ellipsis.Get());
        }
    }
    return layout;
}

void Gfx::Text(const std::wstring& s, const Rect& r, Font f, const Color& c, Align a, bool vcenter, bool wrap) {
    if (s.empty()) return;
    auto layout = Layout(s, f, r.w, r.h, wrap);
    if (!layout) return;
    layout->SetTextAlignment(a == Align::Left ? DWRITE_TEXT_ALIGNMENT_LEADING
                             : a == Align::Center ? DWRITE_TEXT_ALIGNMENT_CENTER
                                                  : DWRITE_TEXT_ALIGNMENT_TRAILING);
    layout->SetParagraphAlignment(vcenter ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    target_->DrawTextLayout(D2D1::Point2F(r.x, r.y), layout.Get(), Brush(c), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float Gfx::MeasureWidth(const std::wstring& s, Font f) {
    auto layout = Layout(s, f, 10000, 1000, false);
    DWRITE_TEXT_METRICS m{};
    if (layout) layout->GetMetrics(&m);
    return m.widthIncludingTrailingWhitespace;
}

float Gfx::MeasureHeight(const std::wstring& s, Font f, float width) {
    auto layout = Layout(s, f, width, 10000, true);
    DWRITE_TEXT_METRICS m{};
    if (layout) layout->GetMetrics(&m);
    return m.height;
}

ID2D1Bitmap* Gfx::Image(const fs::path& p) {
    std::wstring key = p.wstring();
    if (auto it = images_.find(key); it != images_.end()) return it->second.Get();
    images_[key] = nullptr;
    if (!wic_) return nullptr;
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(wic_->CreateDecoderFromFilename(p.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec)))
        return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(dec->GetFrame(0, &frame))) return nullptr;
    ComPtr<IWICFormatConverter> conv;
    wic_->CreateFormatConverter(&conv);
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeMedianCut)))
        return nullptr;
    ComPtr<ID2D1Bitmap> bmp;
    target_->CreateBitmapFromWicBitmap(conv.Get(), &bmp);
    images_[key] = bmp;
    return bmp.Get();
}

ID2D1Bitmap* Gfx::ImageFromResource(int id) {
    std::wstring key = L"#res" + std::to_wstring(id);
    if (auto it = images_.find(key); it != images_.end()) return it->second.Get();
    images_[key] = nullptr;
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), L"PNG");
    if (!res || !wic_) return nullptr;
    HGLOBAL mem = LoadResource(nullptr, res);
    void* data = LockResource(mem);
    DWORD size = SizeofResource(nullptr, res);
    ComPtr<IWICStream> stream;
    wic_->CreateStream(&stream);
    stream->InitializeFromMemory((BYTE*)data, size);
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(wic_->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &dec))) return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame;
    dec->GetFrame(0, &frame);
    ComPtr<IWICFormatConverter> conv;
    wic_->CreateFormatConverter(&conv);
    conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                     WICBitmapPaletteTypeMedianCut);
    ComPtr<ID2D1Bitmap> bmp;
    target_->CreateBitmapFromWicBitmap(conv.Get(), &bmp);
    images_[key] = bmp;
    return bmp.Get();
}

void Gfx::DrawImage(ID2D1Bitmap* bmp, const Rect& r, float opacity, bool cover, float radius) {
    if (!bmp) return;
    auto sz = bmp->GetSize();
    D2D1_RECT_F src = D2D1::RectF(0, 0, sz.width, sz.height);
    Rect dst = r;
    float ar = sz.width / sz.height, rr = r.w / r.h;
    if (cover) {
        if (ar > rr) {
            float w = sz.height * rr;
            src = D2D1::RectF((sz.width - w) / 2, 0, (sz.width + w) / 2, sz.height);
        } else {
            float h = sz.width / rr;
            src = D2D1::RectF(0, (sz.height - h) / 2, sz.width, (sz.height + h) / 2);
        }
    } else {
        if (ar > rr) dst = {r.x, r.y + (r.h - r.w / ar) / 2, r.w, r.w / ar};
        else dst = {r.x + (r.w - r.h * ar) / 2, r.y, r.h * ar, r.h};
    }
    if (radius > 0) PushClip(dst, radius);
    target_->DrawBitmap(bmp, dst.d2d(), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
    if (radius > 0) PopClip();
}

void Gfx::PushClip(const Rect& r, float radius) {
    if (radius <= 0) {
        target_->PushAxisAlignedClip(r.d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        clipIsLayer_.push_back(false);
        return;
    }
    ComPtr<ID2D1RoundedRectangleGeometry> geo;
    factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r.d2d(), radius, radius), &geo);
    target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), geo.Get()), nullptr);
    clipIsLayer_.push_back(true);
}

void Gfx::PopClip() {
    if (clipIsLayer_.empty()) return;
    if (clipIsLayer_.back()) target_->PopLayer();
    else target_->PopAxisAlignedClip();
    clipIsLayer_.pop_back();
}

void Gfx::PushOpacity(float o) {
    target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                             D2D1::IdentityMatrix(), o),
                       nullptr);
    ++opacityDepth_;
}

void Gfx::PopOpacity() {
    if (opacityDepth_ <= 0) return;
    target_->PopLayer();
    --opacityDepth_;
}

void Gfx::SetTransform(float dx, float dy, float scale, float cx, float cy) {
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(cx, cy)) *
                          D2D1::Matrix3x2F::Translation(dx, dy));
}

void Gfx::ResetTransform() { target_->SetTransform(D2D1::Matrix3x2F::Identity()); }

}
