#include "crosshair_renderer.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>
#include <vector>

namespace zext {

namespace {

constexpr UINT kTimerId = 1;
constexpr int kMaxThickness = 3;

const char* shape_name(CrosshairShape shape) {
    switch (shape) {
    case CrosshairShape::Cross:
        return "cross";
    case CrosshairShape::DiagonalCross:
        return "diagonal cross (X)";
    case CrosshairShape::Circle:
        return "circle";
    case CrosshairShape::Dot:
        return "dot";
    case CrosshairShape::RingDot:
        return "ring + dot";
    case CrosshairShape::Triangle:
        return "triangle";
    case CrosshairShape::Brackets:
        return "brackets";
    case CrosshairShape::Square:
        return "square";
    case CrosshairShape::CrossCircle:
        return "cross + circle";
    case CrosshairShape::CrossCircleDot:
        return "cross + circle + dot";
    default:
        return "unknown";
    }
}

struct NamedColor {
    const char* name;
    COLORREF value;
};

const NamedColor kPalette[] = {
    {"white", RGB(255, 255, 255)},
    {"red", RGB(255, 70, 70)},
    {"green", RGB(80, 255, 80)},
    {"cyan", RGB(80, 220, 255)},
    {"yellow", RGB(255, 220, 80)},
    {"orange", RGB(255, 150, 50)},
    {"black", RGB(0, 0, 0)},
};

std::string color_name(COLORREF color) {
    for (const NamedColor& entry : kPalette) {
        if (entry.value == color) {
            return entry.name;
        }
    }
    std::ostringstream stream;
    stream << "rgb(" << static_cast<int>(GetRValue(color)) << ","
           << static_cast<int>(GetGValue(color)) << ","
           << static_cast<int>(GetBValue(color)) << ")";
    return stream.str();
}

void draw_thick_line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color,
                     int thickness, bool ellipse_caps) {
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    if (ellipse_caps) {
        HBRUSH brush = CreateSolidBrush(color);
        HGDIOBJ old_brush = SelectObject(dc, brush);
        const int radius = thickness / 2;
        Ellipse(dc, x1 - radius, y1 - radius, x1 + radius + 1, y1 + radius + 1);
        Ellipse(dc, x2 - radius, y2 - radius, x2 + radius + 1, y2 + radius + 1);
        SelectObject(dc, old_brush);
        DeleteObject(brush);
    }
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void draw_filled_circle(HDC dc, int center_x, int center_y, int radius,
                        COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, center_x - radius, center_y - radius, center_x + radius + 1,
            center_y + radius + 1);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brush);
}

void draw_circle_outline(HDC dc, int center_x, int center_y, int radius,
                         COLORREF color, int thickness) {
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    Ellipse(dc, center_x - radius, center_y - radius, center_x + radius + 1,
            center_y + radius + 1);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
    SelectObject(dc, old_brush);
}

void draw_triangle(HDC dc, int center_x, int center_y, int size, COLORREF color) {
    const double half = size * 0.9;
    POINT points[3] = {
        {center_x, center_y - size},
        {static_cast<LONG>(center_x - half), static_cast<LONG>(center_y + size * 0.8)},
        {static_cast<LONG>(center_x + half), static_cast<LONG>(center_y + size * 0.8)},
    };
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, points, 3);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brush);
}

void draw_brackets(HDC dc, int center_x, int center_y, int size, COLORREF color,
                   int thickness) {
    const int outer = size;
    const int inner = static_cast<int>(size * 0.45);
    draw_thick_line(dc, center_x - outer, center_y - inner, center_x - outer,
                    center_y - outer, color, thickness, false);
    draw_thick_line(dc, center_x - outer, center_y - outer, center_x - inner,
                    center_y - outer, color, thickness, false);
    draw_thick_line(dc, center_x + inner, center_y - outer, center_x + outer,
                    center_y - outer, color, thickness, false);
    draw_thick_line(dc, center_x + outer, center_y - outer, center_x + outer,
                    center_y - inner, color, thickness, false);
    draw_thick_line(dc, center_x + outer, center_y + inner, center_x + outer,
                    center_y + outer, color, thickness, false);
    draw_thick_line(dc, center_x + outer, center_y + outer, center_x + inner,
                    center_y + outer, color, thickness, false);
    draw_thick_line(dc, center_x - inner, center_y + outer, center_x - outer,
                    center_y + outer, color, thickness, false);
    draw_thick_line(dc, center_x - outer, center_y + outer, center_x - outer,
                    center_y + inner, color, thickness, false);
}

} // namespace

bool CrosshairRenderer::init(HINSTANCE instance) {
    instance_ = instance;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &CrosshairRenderer::wnd_proc;
    window_class.hInstance = instance_;
    window_class.lpszClassName = L"ZetianCrosshairOverlay";
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (RegisterClassExW(&window_class) == 0) {
        return false;
    }

    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW
            | WS_EX_NOACTIVATE,
        window_class.lpszClassName, L"", WS_POPUP, 0, 0, screen_width,
        screen_height, nullptr, nullptr, instance_, this);

    if (hwnd_ == nullptr) {
        return false;
    }

    SetLayeredWindowAttributes(hwnd_, kColorKey, 0, LWA_COLORKEY);
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    SetTimer(hwnd_, kTimerId, 33, nullptr);
    ensure_back_buffer(screen_width, screen_height);
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void CrosshairRenderer::shutdown() {
    if (hwnd_ != nullptr) {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
}

int CrosshairRenderer::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK CrosshairRenderer::wnd_proc(HWND hwnd, UINT message,
                                             WPARAM wparam, LPARAM lparam) {
    CrosshairRenderer* self = reinterpret_cast<CrosshairRenderer*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }
    case WM_TIMER:
        if (self != nullptr) {
            self->on_timer();
        }
        return 0;
    case WM_DISPLAYCHANGE: {
        const int width = LOWORD(lparam);
        const int height = HIWORD(lparam);
        if (self != nullptr) {
            self->ensure_back_buffer(width, height);
            SetWindowPos(hwnd, nullptr, 0, 0, width, height,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void CrosshairRenderer::on_timer() {
    HDC window_dc = GetDC(hwnd_);
    if (window_dc != nullptr) {
        render(window_dc);
        ReleaseDC(hwnd_, window_dc);
    }
}

void CrosshairRenderer::render(HDC target) {
    if (back_dc_ == nullptr || back_bitmap_ == nullptr) {
        return;
    }

    HBRUSH key_brush = CreateSolidBrush(kColorKey);
    HGDIOBJ old_brush = SelectObject(back_dc_, key_brush);
    HGDIOBJ old_pen = SelectObject(back_dc_, GetStockObject(NULL_PEN));
    Rectangle(back_dc_, 0, 0, back_width_, back_height_);
    SelectObject(back_dc_, old_pen);
    SelectObject(back_dc_, old_brush);
    DeleteObject(key_brush);

    CrosshairStyle style;
    {
        std::lock_guard<std::mutex> lock(style_mutex_);
        style = style_;
    }

    const int center_x = back_width_ / 2;
    const int center_y = back_height_ / 2;
    draw_shape(back_dc_, center_x, center_y, style);

    BitBlt(target, 0, 0, back_width_, back_height_, back_dc_, 0, 0, SRCCOPY);
}

void CrosshairRenderer::draw_shape(HDC dc, int center_x, int center_y,
                                   const CrosshairStyle& style) {
    const int size = std::max(4, style.size);
    const int thickness = std::clamp(style.thickness, 1, kMaxThickness);
    const COLORREF color = style.color;

    switch (style.shape) {
    case CrosshairShape::Cross:
        draw_thick_line(dc, center_x, center_y - size, center_x,
                        center_y + size, color, thickness, true);
        draw_thick_line(dc, center_x - size, center_y, center_x + size,
                        center_y, color, thickness, true);
        break;
    case CrosshairShape::DiagonalCross:
        draw_thick_line(dc, center_x - size, center_y - size, center_x + size,
                        center_y + size, color, thickness, true);
        draw_thick_line(dc, center_x - size, center_y + size, center_x + size,
                        center_y - size, color, thickness, true);
        break;
    case CrosshairShape::Circle:
        draw_circle_outline(dc, center_x, center_y, size, color, thickness);
        break;
    case CrosshairShape::Dot:
        draw_filled_circle(dc, center_x, center_y, std::max(2, size / 2), color);
        break;
    case CrosshairShape::RingDot:
        draw_circle_outline(dc, center_x, center_y, size, color, thickness);
        draw_filled_circle(dc, center_x, center_y, std::max(2, size / 4), color);
        break;
    case CrosshairShape::Triangle:
        draw_triangle(dc, center_x, center_y, size, color);
        break;
    case CrosshairShape::Brackets:
        draw_brackets(dc, center_x, center_y, size, color, thickness);
        break;
    case CrosshairShape::Square: {
        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        HPEN pen = CreatePen(PS_SOLID, thickness, color);
        HGDIOBJ old_pen = SelectObject(dc, pen);
        Rectangle(dc, center_x - size, center_y - size, center_x + size + 1,
                  center_y + size + 1);
        SelectObject(dc, old_pen);
        DeleteObject(pen);
        SelectObject(dc, old_brush);
        break;
    }
    case CrosshairShape::CrossCircle:
        draw_thick_line(dc, center_x, center_y - size, center_x,
                        center_y + size, color, thickness, true);
        draw_thick_line(dc, center_x - size, center_y, center_x + size,
                        center_y, color, thickness, true);
        draw_circle_outline(dc, center_x, center_y,
                            static_cast<int>(size * 1.45), color, thickness);
        break;
    case CrosshairShape::CrossCircleDot:
        draw_thick_line(dc, center_x, center_y - size, center_x,
                        center_y + size, color, thickness, true);
        draw_thick_line(dc, center_x - size, center_y, center_x + size,
                        center_y, color, thickness, true);
        draw_circle_outline(dc, center_x, center_y,
                            static_cast<int>(size * 1.45), color, thickness);
        draw_filled_circle(dc, center_x, center_y, std::max(2, size / 4), color);
        break;
    default:
        break;
    }
}

void CrosshairRenderer::ensure_back_buffer(int width, int height) {
    if (back_dc_ != nullptr && back_width_ == width && back_height_ == height) {
        return;
    }
    if (back_old_ != nullptr) {
        SelectObject(back_dc_, back_old_);
        back_old_ = nullptr;
    }
    if (back_bitmap_ != nullptr) {
        DeleteObject(back_bitmap_);
        back_bitmap_ = nullptr;
    }
    if (back_dc_ != nullptr) {
        DeleteDC(back_dc_);
        back_dc_ = nullptr;
    }
    HDC screen_dc = GetDC(nullptr);
    if (screen_dc == nullptr) {
        return;
    }
    back_dc_ = CreateCompatibleDC(screen_dc);
    if (back_dc_ != nullptr) {
        back_bitmap_ = CreateCompatibleBitmap(screen_dc, width, height);
        if (back_bitmap_ != nullptr) {
            back_old_ = static_cast<HBITMAP>(
                SelectObject(back_dc_, back_bitmap_));
            back_width_ = width;
            back_height_ = height;
        }
    }
    ReleaseDC(nullptr, screen_dc);
}

CrosshairStyle CrosshairRenderer::random_style() {
    CrosshairStyle style;
    style.shape = static_cast<CrosshairShape>(
        std::uniform_int_distribution<int>(0,
            static_cast<int>(CrosshairShape::ShapeCount) - 1)(rng_));
    style.size = std::uniform_int_distribution<int>(14, 44)(rng_);
    style.thickness = std::uniform_int_distribution<int>(1, kMaxThickness)(rng_);
    style.color = kPalette[std::uniform_int_distribution<std::size_t>(
                               0, std::size(kPalette) - 1)(rng_)].value;
    return style;
}

std::string CrosshairRenderer::describe(const CrosshairStyle& style) const {
    std::ostringstream stream;
    stream << shape_name(style.shape) << " (size " << style.size
           << ", thickness " << style.thickness << ", color "
           << color_name(style.color) << ")";
    return stream.str();
}

std::string CrosshairRenderer::describe_style(const CrosshairStyle& style) const {
    return describe(style);
}

std::string CrosshairRenderer::request_random_style() {
    std::lock_guard<std::mutex> lock(style_mutex_);
    style_ = random_style();
    return describe(style_);
}

} // namespace zext
