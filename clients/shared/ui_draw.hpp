// ui_draw.hpp — lista poleceń rysowania UI (prostokąty + tekst 5x7),
// niezależna od platformy. VulkanUi zamienia ją na wierzchołki Vulkan.
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "font5x7.hpp"

struct UiColor {
    float r = 1, g = 1, b = 1, a = 1;
    constexpr UiColor() = default;
    constexpr UiColor(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
};

namespace ui {
    inline const UiColor BG{0.035f, 0.05f, 0.09f};
    inline const UiColor PANEL{0.07f, 0.10f, 0.17f};
    inline const UiColor CARD{0.10f, 0.14f, 0.23f};
    inline const UiColor CARD_SEL{0.16f, 0.20f, 0.32f};
    inline const UiColor ACCENT{0.98f, 0.45f, 0.09f};
    inline const UiColor ACCENT2{0.13f, 0.55f, 0.30f};
    inline const UiColor TEXT{0.94f, 0.96f, 1.0f};
    inline const UiColor MUTED{0.62f, 0.68f, 0.79f};
    inline const UiColor WARN{0.96f, 0.68f, 0.15f};
    inline const UiColor BAD{0.92f, 0.30f, 0.27f};
    inline const UiColor BLUE{0.30f, 0.55f, 0.95f};
}

// Zamienia UTF-8 na ASCII wielkimi literami (font 5x7): ą->A, ś->S, ł->L itd.
inline std::string uiSanitize(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size();) {
        unsigned char c = (unsigned char)in[i];
        unsigned cp = c; size_t len = 1;
        if ((c & 0xE0) == 0xC0 && i + 1 < in.size()) { cp = ((c & 0x1F) << 6) | ((unsigned char)in[i+1] & 0x3F); len = 2; }
        else if ((c & 0xF0) == 0xE0 && i + 2 < in.size()) { cp = ((c & 0x0F) << 12) | (((unsigned char)in[i+1] & 0x3F) << 6) | ((unsigned char)in[i+2] & 0x3F); len = 3; }
        i += len;
        char rep = '?';
        if (cp < 128) rep = (char)cp;
        else {
            switch (cp) {
                case 0x105: case 0x104: rep = 'A'; break; // ą Ą
                case 0x107: case 0x106: rep = 'C'; break; // ć Ć
                case 0x119: case 0x118: rep = 'E'; break; // ę Ę
                case 0x142: case 0x141: rep = 'L'; break; // ł Ł
                case 0x144: case 0x143: rep = 'N'; break; // ń Ń
                case 0xF3: case 0xD3: rep = 'O'; break;   // ó Ó
                case 0x15B: case 0x15A: rep = 'S'; break; // ś Ś
                case 0x17A: case 0x179: case 0x17C: case 0x17B: rep = 'Z'; break; // ź Ź ż Ż
                default: rep = '?'; break;
            }
        }
        if (rep >= 'a' && rep <= 'z') rep = char(rep - 'a' + 'A');
        if (rep == '\n' || rep == '\t') rep = ' ';
        out += rep;
    }
    return out;
}

inline float uiTextWidth(const std::string& text, float scale) {
    return (float)uiSanitize(text).size() * 6.0f * scale;
}

// Skraca tekst do maxWidth, dodając ".." na końcu.
inline std::string uiClip(const std::string& text, float scale, float maxWidth) {
    std::string s = uiSanitize(text);
    if (maxWidth <= 0) return s;
    size_t maxChars = (size_t)(maxWidth / (6.0f * scale));
    if (s.size() <= maxChars) return s;
    if (maxChars <= 2) return s.substr(0, maxChars);
    return s.substr(0, maxChars - 2) + "..";
}

struct UiRectCmd { float x, y, w, h; UiColor color; };
struct UiTextCmd { float x, y, scale; UiColor color; std::string text; float maxWidth = 0; };

struct DrawList {
    std::vector<UiRectCmd> rects;
    std::vector<UiTextCmd> texts;
    void clear() { rects.clear(); texts.clear(); }
    void rect(float x, float y, float w, float h, UiColor c) {
        if (w > 0 && h > 0) rects.push_back({x, y, w, h, c});
    }
    void outline(float x, float y, float w, float h, float t, UiColor c) {
        rect(x, y, w, t, c); rect(x, y + h - t, w, t, c); rect(x, y, t, h, c); rect(x + w - t, y, t, h, c);
    }
    // scale = rozmiar piksela glifu (2 => znak 10x14 px, krok 12 px)
    void text(float x, float y, float scale, UiColor c, const std::string& s, float maxWidth = 0) {
        texts.push_back({x, y, scale, c, uiClip(s, scale, maxWidth), maxWidth});
    }
};
