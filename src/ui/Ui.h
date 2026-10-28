#pragma once
#include <functional>
#include <string>
#include <unordered_map>

#include "ui/Gfx.h"

namespace sk::ui {

struct Theme {
    Color bg = Color::Hex(0x0B0910);
    Color panel = Color::Hex(0x15121C);
    Color panel2 = Color::Hex(0x1C1825);
    Color line = Color::Hex(0xFFFFFF, 0.07f);
    Color text = Color::Hex(0xF4F1F8);
    Color muted = Color::Hex(0xA49DB2);
    Color faint = Color::Hex(0x6E6880);
    Color accent = Color::Hex(0xFF2E4D);
    Color accent2 = Color::Hex(0xFF8A3D);
    Color ok = Color::Hex(0x3DDC97);
    Color warn = Color::Hex(0xFFC145);
    Color bad = Color::Hex(0xFF4D6A);
};

struct Input {
    float mx = -1, my = -1;
    float px = -1, py = -1;
    bool down = false, pressed = false, released = false;
    float wheel = 0;
    bool rightPressed = false;
    std::wstring chars;
    int backspaces = 0;
    bool enter = false;
    bool escape = false;
    bool selectAll = false;
};

enum class Style { Primary, Secondary, Ghost, Danger };

class Ui {
public:
    Gfx& g;
    Theme theme, targetTheme;
    Input in;
    float dt = 0.016f;
    double time = 0;
    bool wantsCursorHand = false;
    bool animating = false;

    explicit Ui(Gfx& gfx) : g(gfx) {}

    void BeginFrame(float dtSeconds);
    void EndFrame();
    void SetTheme(const Theme& t) { targetTheme = t; }

    float Anim(const std::string& id, float target, float speed = 14.f);
    void Snap(const std::string& id, float value) { anims_[id] = value; }
    float Peek(const std::string& id) const {
        auto it = anims_.find(id);
        return it == anims_.end() ? 0.f : it->second;
    }
    bool Hover(const Rect& r) const { return !blocked_ && r.contains(in.mx, in.my); }
    bool Clicked(const Rect& r) { return Hover(r) && in.released && pressedIn_(r); }

    bool Button(const std::string& id, const Rect& r, const std::wstring& label, Style s = Style::Primary,
                bool enabled = true, const std::wstring& icon = {});
    bool IconButton(const std::string& id, const Rect& r, const std::wstring& glyph, const Color& hoverBg,
                    const Color& fg);
    bool Toggle(const std::string& id, const Rect& r, bool& value, bool enabled = true);
    bool Slider(const std::string& id, const Rect& r, float& value, float min, float max);
    void Meter(const Rect& r, float level);
    bool TextField(const std::string& id, const Rect& r, std::wstring& value, const std::wstring& placeholder,
                   bool secret = false, size_t maxLen = 300);
    bool HasFocus() const { return !focus_.empty(); }
    float PressX() const { return pressX_; }
    float PressY() const { return pressY_; }
    void ClearFocus() { focus_.clear(); }
    bool SelectCard(const std::string& id, const Rect& r, bool selected, const Color& accent);
    bool NavItem(const std::string& id, const Rect& r, const std::wstring& glyph, const std::wstring& label,
                 bool active, const std::wstring& badge = {});
    void Progress(const Rect& r, float value, bool indeterminate = false);
    void Spinner(float cx, float cy, float radius);
    void Badge(const Rect& r, const std::wstring& text, const Color& c);
    void Panel(const Rect& r, float radius = 14);
    void Check(float cx, float cy, float size, const Color& c, float progress = 1);
    float Scroll(const std::string& id, const Rect& viewport, float contentHeight);
    void Tooltip(const Rect& anchor, const std::wstring& text);

    void Block(bool b) { blocked_ = b; }
    bool Blocked() const { return blocked_; }

private:
    bool pressedIn_(const Rect& r) const { return pressX_ >= r.x && pressX_ < r.r() && pressY_ >= r.y && pressY_ < r.b(); }

    std::unordered_map<std::string, float> anims_;
    float pressX_ = -1, pressY_ = -1;
    std::string dragging_;
    std::string focus_;
    bool focusClaimed_ = false;
    std::unordered_map<std::string, bool> reveal_;
    std::unordered_map<std::string, bool> selected_;
    bool blocked_ = false;
    std::wstring tooltip_;
    Rect tooltipAnchor_;
};

}
