#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mutex>
#include <random>
#include <string>

namespace zext {

enum class CrosshairShape {
    Cross,
    DiagonalCross,
    Circle,
    Dot,
    RingDot,
    Triangle,
    Brackets,
    Square,
    CrossCircle,
    CrossCircleDot,
    ShapeCount,
};

struct CrosshairStyle {
    CrosshairShape shape = CrosshairShape::Cross;
    int size = 24;
    int thickness = 2;
    COLORREF color = RGB(255, 255, 255);
};

class CrosshairRenderer {
public:
    bool init(HINSTANCE instance);
    void shutdown();
    int run();
    HWND hwnd() const { return hwnd_; }

    std::string request_random_style();
    std::string describe_style(const CrosshairStyle& style) const;

private:
    static constexpr COLORREF kColorKey = RGB(255, 0, 255);

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    void on_timer();
    void render(HDC target);
    void draw_shape(HDC dc, int center_x, int center_y, const CrosshairStyle& style);
    void ensure_back_buffer(int width, int height);
    CrosshairStyle random_style();
    std::string describe(const CrosshairStyle& style) const;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HDC back_dc_ = nullptr;
    HBITMAP back_bitmap_ = nullptr;
    HBITMAP back_old_ = nullptr;
    int back_width_ = 0;
    int back_height_ = 0;

    mutable std::mutex style_mutex_;
    CrosshairStyle style_;
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace zext
