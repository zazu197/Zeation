#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace Gdiplus {
class Bitmap;
}

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
    int image_index = -1;
};

struct CustomCrosshairImage {
    std::string name;
    int width = 0;
    int height = 0;
    std::shared_ptr<Gdiplus::Bitmap> bitmap;
};

class CrosshairRenderer {
public:
    ~CrosshairRenderer();

    bool init(HINSTANCE instance);
    void shutdown();
    int run();
    HWND hwnd() const { return hwnd_; }

    bool load_custom_images(const std::string& folder);
    std::size_t custom_image_count() const { return images_.size(); }
    bool has_custom_images() const { return !images_.empty(); }

    std::string request_random_style();
    std::string describe_style(const CrosshairStyle& style) const;
    bool save_snapshot(const std::string& path);

private:
    static constexpr COLORREF kColorKey = RGB(255, 0, 255);

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    void on_timer();
    void draw_back_buffer();
    void render(HDC target);
    bool create_overlay_window();
    void cleanup_back_buffer();
    void draw_shape(HDC dc, int center_x, int center_y, const CrosshairStyle& style);
    void draw_image(HDC dc, int center_x, int center_y, const CrosshairStyle& style);
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

    std::vector<CustomCrosshairImage> images_;
    ULONG_PTR gdiplus_token_ = 0;

    mutable std::mutex style_mutex_;
    CrosshairStyle style_;
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace zext
