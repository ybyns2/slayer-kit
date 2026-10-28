#include "ui/Ui.h"

#include <algorithm>
#include <cmath>

namespace sk::ui {

namespace {
Color Lerp(const Color& a, const Color& b, float t) { return a.mix(b, t); }
bool Near(const Color& a, const Color& b) {
    return std::fabs(a.r - b.r) + std::fabs(a.g - b.g) + std::fabs(a.b - b.b) < 0.004f;
}
}

void Ui::BeginFrame(float dtSeconds) {
    dt = std::clamp(dtSeconds, 0.001f, 0.05f);
    time += dt;
    animating = false;
    wantsCursorHand = false;
    tooltip_.clear();
    if (in.pressed) {
        pressX_ = in.px >= 0 ? in.px : in.mx;
        pressY_ = in.py >= 0 ? in.py : in.my;
    }
    focusClaimed_ = false;
    float t = 1 - std::exp(-dt * 6.f);
    auto step = [&](Color& c, const Color& target) {
        if (!Near(c, target)) {
            c = Lerp(c, target, t);
            animating = true;
        } else c = target;
    };
    step(theme.bg, targetTheme.bg);
    step(theme.panel, targetTheme.panel);
    step(theme.panel2, targetTheme.panel2);
    step(theme.accent, targetTheme.accent);
    step(theme.accent2, targetTheme.accent2);
}

void Ui::EndFrame() {
    if (!tooltip_.empty()) {
        float w = std::min(g.MeasureWidth(tooltip_, Font::Small) + 24, 340.f);
        float h = g.MeasureHeight(tooltip_, Font::Small, w - 24) + 16;
        Rect r{std::clamp(tooltipAnchor_.x + tooltipAnchor_.w / 2 - w / 2, 8.f, g.Width() - w - 8),
               tooltipAnchor_.y - h - 8, w, h};
        if (r.y < 8) r.y = tooltipAnchor_.b() + 8;
        g.Glow(r, Color{0, 0, 0, 1}, 8, 14);
        g.Fill(r, theme.panel2.lighten(0.04f), 8);
        g.Stroke(r, theme.line, 8);
        g.Text(tooltip_, r.inset(12, 8), Font::Small, theme.text, Align::Left, false, true);
    }
    if (in.pressed && !focusClaimed_) focus_.clear();
    if (in.escape) focus_.clear();
    if (in.released) pressX_ = pressY_ = -1;
    in.pressed = in.released = in.rightPressed = false;
    in.chars.clear();
    in.backspaces = 0;
    in.enter = in.escape = in.selectAll = false;
    in.wheel = 0;
}

float Ui::Anim(const std::string& id, float target, float speed) {
    auto [it, inserted] = anims_.try_emplace(id, target);
    float& v = it->second;
    if (inserted) return v;
    float k = 1 - std::exp(-dt * speed);
    v += (target - v) * k;
    if (std::fabs(target - v) < 0.002f) v = target;
    else animating = true;
    return v;
}

bool Ui::Button(const std::string& id, const Rect& r, const std::wstring& label, Style s, bool enabled,
                const std::wstring& icon) {
    bool hover = enabled && Hover(r);
    bool down = hover && in.down && pressedIn_(r);
    float h = Anim(id + "#h", hover ? 1.f : 0.f);
    float p = Anim(id + "#p", down ? 1.f : 0.f, 30);
    if (hover) wantsCursorHand = true;

    float scale = 1 - 0.03f * p;
    Rect rr{r.x + r.w * (1 - scale) / 2, r.y + r.h * (1 - scale) / 2, r.w * scale, r.h * scale};
    float rad = std::min(10.f, rr.h / 2);
    Color fg = theme.text;
    switch (s) {
    case Style::Primary:
        if (enabled) g.Glow(rr, theme.accent, rad, 6 + 10 * h);
        g.FillGradient(rr, enabled ? theme.accent.lighten(0.08f * h) : theme.panel2,
                       enabled ? theme.accent2.lighten(0.08f * h) : theme.panel2, false, rad);
        fg = enabled ? Color{1, 1, 1, 1} : theme.faint;
        break;
    case Style::Secondary:
        g.Fill(rr, theme.panel2.lighten(0.05f + 0.06f * h), rad);
        g.Stroke(rr, theme.line.alpha(1 + h), rad);
        fg = enabled ? theme.text : theme.faint;
        break;
    case Style::Ghost:
        if (h > 0.01f) g.Fill(rr, Color{1, 1, 1, 0.06f * h}, rad);
        fg = enabled ? theme.muted.mix(theme.text, h) : theme.faint;
        break;
    case Style::Danger:
        g.Fill(rr, theme.bad.alpha(0.14f + 0.12f * h), rad);
        g.Stroke(rr, theme.bad.alpha(0.5f), rad);
        fg = theme.bad.lighten(0.3f);
        break;
    }
    if (!icon.empty()) {
        float tw = g.MeasureWidth(label, Font::BodyBold);
        float total = tw + 24;
        float sx = rr.x + (rr.w - total) / 2;
        g.Text(icon, {sx, rr.y, 18, rr.h}, Font::Icon, fg, Align::Left);
        g.Text(label, {sx + 24, rr.y, tw + 2, rr.h}, Font::BodyBold, fg, Align::Left);
    } else {
        g.Text(label, rr, Font::BodyBold, fg, Align::Center);
    }
    return enabled && Clicked(r);
}

bool Ui::IconButton(const std::string& id, const Rect& r, const std::wstring& glyph, const Color& hoverBg,
                    const Color& fg) {
    bool hover = Hover(r);
    float h = Anim(id + "#h", hover ? 1.f : 0.f, 20);
    if (h > 0.01f) g.Fill(r, hoverBg.alpha(hoverBg.a * h));
    g.Text(glyph, r, Font::Icon, fg.mix(Color{1, 1, 1, 1}, h * 0.5f), Align::Center);
    return Clicked(r);
}

bool Ui::Toggle(const std::string& id, const Rect& r, bool& value, bool enabled) {
    Rect track{r.x, r.y + (r.h - 22) / 2, 40, 22};
    bool hover = enabled && Hover(r);
    if (hover) wantsCursorHand = true;
    float t = Anim(id + "#t", value ? 1.f : 0.f, 18);
    float h = Anim(id + "#h", hover ? 1.f : 0.f);
    g.Fill(track, theme.panel2.lighten(0.08f + 0.05f * h).mix(theme.accent, t), 11);
    if (!value) g.Stroke(track, theme.line.alpha(1.5f), 11);
    float kx = track.x + 11 + t * 18;
    g.Circle(kx, track.y + 11, 8 + h, enabled ? Color{1, 1, 1, 1} : theme.faint);
    bool changed = enabled && Clicked(r);
    if (changed) value = !value;
    return changed;
}

bool Ui::Slider(const std::string& id, const Rect& r, float& value, float min, float max) {
    Rect track{r.x, r.y + r.h / 2 - 3, r.w, 6};
    bool hover = Hover(r);
    if (hover) wantsCursorHand = true;
    if (hover && in.pressed) dragging_ = id;
    if (!in.down && dragging_ == id) dragging_.clear();
    bool changed = false;
    if (dragging_ == id) {
        float t = std::clamp((in.mx - r.x) / r.w, 0.f, 1.f);
        float v = min + (max - min) * t;
        if (std::fabs(v - value) > 1e-4f) {
            value = v;
            changed = true;
        }
    }
    float t = (value - min) / (max - min);
    float h = Anim(id + "#h", hover || dragging_ == id ? 1.f : 0.f);
    g.Fill(track, theme.panel2.lighten(0.1f), 3);
    g.FillGradient({track.x, track.y, std::max(6.f, track.w * t), track.h}, theme.accent, theme.accent2, false, 3);
    g.Circle(track.x + track.w * t, track.y + 3, 8 + 2 * h, Color{1, 1, 1, 1});
    return changed;
}

void Ui::Meter(const Rect& r, float level) {
    g.Fill(r, theme.panel2.lighten(0.06f), r.h / 2);
    float db = level <= 0.00001f ? -60.f : 20.f * std::log10(level);
    float t = std::clamp((db + 60.f) / 60.f, 0.f, 1.f);
    float v = Anim("meter#" + std::to_string((int)r.x) + "_" + std::to_string((int)r.y), t, 30);
    Color c = v > 0.92f ? theme.bad : v > 0.75f ? theme.warn : theme.ok;
    if (v > 0.01f) g.Fill({r.x, r.y, r.w * v, r.h}, c, r.h / 2);
}

bool Ui::TextField(const std::string& id, const Rect& r, std::wstring& value, const std::wstring& placeholder, bool secret,
                   size_t maxLen) {
    bool hover = Hover(r);
    Rect eye{r.r() - 40, r.y, 40, r.h};
    if (hover && in.pressed) {
        focusClaimed_ = true;
        if (!(secret && eye.contains(in.mx, in.my))) focus_ = id;
    }
    if (secret && Clicked(eye)) reveal_[id] = !reveal_[id];
    bool focused = focus_ == id;
    bool changed = false;
    if (focused) {
        if (in.selectAll) selected_[id] = true;
        if (selected_[id] && (in.backspaces > 0 || !in.chars.empty())) {
            value.clear();
            selected_[id] = false;
            changed = true;
            if (in.backspaces > 0) in.backspaces = 0;
        }
        for (int i = 0; i < in.backspaces && !value.empty(); ++i) {
            value.pop_back();
            changed = true;
        }
        for (wchar_t ch : in.chars) {
            if (ch >= 32 && value.size() < maxLen) {
                value.push_back(ch);
                changed = true;
            }
        }
        if (in.enter) focus_.clear();
    } else {
        selected_[id] = false;
    }
    float f = Anim(id + "#f", focused ? 1.f : 0.f, 16);
    float h = Anim(id + "#h", hover ? 1.f : 0.f);
    g.Fill(r, theme.panel2.lighten(0.03f + 0.03f * h), 10);
    g.Stroke(r, theme.line.alpha(1.5f + h).mix(theme.accent, f), 10, 1 + f);
    Rect tr{r.x + 14, r.y, r.w - (secret ? 60 : 28), r.h};
    std::wstring shown = value;
    if (secret && !reveal_[id]) shown.assign(value.size(), L'•');
    if (value.empty() && !focused) {
        g.Text(placeholder, tr, Font::Small, theme.faint);
    } else {
        float w = g.MeasureWidth(shown, Font::Body);
        g.PushClip(tr);
        float ox = std::min(0.f, tr.w - w - 4);
        if (selected_[id] && !shown.empty()) g.Fill({tr.x + ox, r.y + 8, w, r.h - 16}, theme.accent.alpha(0.35f), 4);
        g.Text(shown, {tr.x + ox, tr.y, std::max(w + 8, tr.w), tr.h}, Font::Body, theme.text);
        if (focused && std::fmod(time, 1.0) < 0.55)
            g.Fill({tr.x + ox + w + 1, r.y + 10, 2, r.h - 20}, theme.accent.lighten(0.3f), 1);
        g.PopClip();
        if (focused) animating = true;
    }
    if (secret) g.Text(reveal_[id] ? L"" : L"", eye, Font::Icon, theme.muted, Align::Center);
    if (hover) wantsCursorHand = true;
    return changed;
}

bool Ui::SelectCard(const std::string& id, const Rect& r, bool selected, const Color& accent) {
    bool hover = Hover(r);
    if (hover) wantsCursorHand = true;
    float h = Anim(id + "#h", hover ? 1.f : 0.f);
    float s = Anim(id + "#s", selected ? 1.f : 0.f, 12);
    Rect rr = r.offset(0, -3 * h);
    if (s > 0.01f) g.Glow(rr, accent, 16, 18 * s);
    g.Fill(rr, theme.panel.lighten(0.03f * h).mix(accent.darken(0.75f), 0.35f * s), 16);
    g.Stroke(rr, theme.line.mix(accent, s).alpha(1 + 0.6f * s + h), 16, 1 + s);
    if (s > 0.01f) {
        float cx = rr.r() - 22, cy = rr.y + 22;
        g.Circle(cx, cy, 11 * s, accent);
        Check(cx, cy, 11, Color{1, 1, 1, 1}, s);
    }
    return Clicked(r);
}

bool Ui::NavItem(const std::string& id, const Rect& r, const std::wstring& glyph, const std::wstring& label,
                 bool active, const std::wstring& badge) {
    bool hover = Hover(r);
    if (hover) wantsCursorHand = true;
    float h = Anim(id + "#h", hover ? 1.f : 0.f);
    float a = Anim(id + "#a", active ? 1.f : 0.f, 12);
    if (h + a > 0.01f) g.Fill(r, Color{1, 1, 1, 0.04f * h}.mix(theme.accent.alpha(0.16f), a), 10);
    if (a > 0.01f) g.Fill({r.x, r.y + r.h * (0.5f - 0.3f * a), 3, r.h * 0.6f * a}, theme.accent, 2);
    Color fg = theme.muted.mix(theme.text, std::max(h, a));
    g.Text(glyph, {r.x + 16, r.y, 20, r.h}, Font::Icon, a > 0.5f ? theme.accent.lighten(0.2f) : fg);
    float bw = badge.empty() ? 0.f : g.MeasureWidth(badge, Font::Caption) + 12;
    g.Text(label, {r.x + 46, r.y, r.w - 56 - (bw > 0 ? bw + 8 : 0), r.h}, active ? Font::BodyBold : Font::Body, fg);
    if (bw > 0) Badge({r.r() - bw - 10, r.y + (r.h - 18) / 2, bw, 18}, badge, theme.accent2);
    return Clicked(r);
}

void Ui::Progress(const Rect& r, float value, bool indeterminate) {
    g.Fill(r, theme.panel2.lighten(0.06f), r.h / 2);
    if (indeterminate) {
        float t = (float)std::fmod(time * 0.8, 1.0);
        float w = r.w * 0.3f;
        float x = r.x - w + (r.w + w) * t;
        g.PushClip(r, r.h / 2);
        g.FillGradient({x, r.y, w, r.h}, theme.accent.alpha(0), theme.accent2, false, r.h / 2);
        g.PopClip();
        animating = true;
        return;
    }
    float v = std::clamp(value, 0.f, 1.f);
    if (v > 0.001f) g.FillGradient({r.x, r.y, std::max(r.h, r.w * v), r.h}, theme.accent, theme.accent2, false, r.h / 2);
}

void Ui::Spinner(float cx, float cy, float radius) {
    float start = (float)std::fmod(time * 360.0, 360.0);
    g.Ring(cx, cy, radius, theme.line.alpha(2), 3);
    g.Arc(cx, cy, radius, start, 100, theme.accent, 3);
    animating = true;
}

void Ui::Badge(const Rect& r, const std::wstring& text, const Color& c) {
    g.Fill(r, c.alpha(0.16f), r.h / 2);
    g.Text(text, r, Font::Caption, c.lighten(0.25f), Align::Center);
}

void Ui::Panel(const Rect& r, float radius) {
    g.Fill(r, theme.panel, radius);
    g.Stroke(r, theme.line, radius);
}

void Ui::Check(float cx, float cy, float size, const Color& c, float progress) {
    float s = size * 0.45f;
    D2D1_POINT_2F a{cx - s, cy}, b{cx - s * 0.3f, cy + s * 0.7f}, e{cx + s, cy - s * 0.6f};
    float p = std::clamp(progress, 0.f, 1.f);
    float first = std::min(p * 2, 1.f), second = std::max(p * 2 - 1, 0.f);
    g.Line(a.x, a.y, a.x + (b.x - a.x) * first, a.y + (b.y - a.y) * first, c, 2.2f);
    if (second > 0) g.Line(b.x, b.y, b.x + (e.x - b.x) * second, b.y + (e.y - b.y) * second, c, 2.2f);
}

float Ui::Scroll(const std::string& id, const Rect& viewport, float contentHeight) {
    float maxScroll = std::max(0.f, contentHeight - viewport.h);
    auto [it, _] = anims_.try_emplace(id + "#target", 0.f);
    float& target = it->second;
    if (Hover(viewport) && in.wheel != 0) target -= in.wheel * 60;
    target = std::clamp(target, 0.f, maxScroll);
    float v = Anim(id + "#pos", target, 16);
    if (maxScroll > 0) {
        float th = std::max(30.f, viewport.h * viewport.h / contentHeight);
        float ty = viewport.y + (viewport.h - th) * (v / maxScroll);
        g.Fill({viewport.r() - 5, ty, 3, th}, theme.text.alpha(0.18f), 2);
    }
    return v;
}

void Ui::Tooltip(const Rect& anchor, const std::wstring& text) {
    if (Hover(anchor)) {
        tooltip_ = text;
        tooltipAnchor_ = anchor;
    }
}

}
