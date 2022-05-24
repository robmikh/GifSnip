#include "pch.h"
#include "MainWindow.h"

namespace winrt
{
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
    using namespace Windows::System;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::uwp;
}

const std::wstring MainWindow::ClassName = L"GifSnip.MainWindow";
const float MainWindow::BorderThickness = 5;
std::once_flag MainWindowClassRegistration;

RECT ComputeAllDisplaysUnion()
{
    RECT result = {};
    result.left = LONG_MAX;
    result.top = LONG_MAX;
    result.right = LONG_MIN;
    result.bottom = LONG_MIN;
    auto infos = util::DisplayInfo::GetAllDisplays();
    for (auto&& info : infos)
    {
        auto rect = info.Rect();
        result.left = std::min(result.left, rect.left);
        result.top = std::min(result.top, rect.top);
        result.right = std::max(result.right, rect.right);
        result.bottom = std::max(result.bottom, rect.bottom);
    }
    return result;
}

void MainWindow::RegisterWindowClass()
{
    auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIconW(instance, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.lpszClassName = ClassName.c_str();
    wcex.hIconSm = LoadIconW(instance, IDI_APPLICATION);
    winrt::check_bool(RegisterClassExW(&wcex));
}

MainWindow::MainWindow(winrt::Compositor const& compositor)
{
    auto instance = winrt::check_pointer(GetModuleHandleW(nullptr));
    m_compositor = compositor;
    m_mainThread = winrt::DispatcherQueue::GetForCurrentThread();

    std::call_once(MainWindowClassRegistration, []() { RegisterWindowClass(); });

    // Window Styles
    auto exStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST;
    auto style = WS_POPUP;

    // Calculate the position and size of the window
    RECT rect = ComputeAllDisplaysUnion();
    m_unionRect = rect;

    // Create the window
    winrt::check_bool(CreateWindowExW(exStyle, ClassName.c_str(), L"", style,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, this));
    WINRT_ASSERT(m_window);

    // Create the visual tree
    m_target = CreateWindowTarget(m_compositor);
    m_root = m_compositor.CreateSpriteVisual();
    m_root.RelativeSizeAdjustment({ 1, 1 });
    m_root.Brush(m_compositor.CreateColorBrush(winrt::Color{ 0, 0, 0, 0 }));
    m_snipVisual = m_compositor.CreateSpriteVisual();
    auto brush = m_compositor.CreateNineGridBrush();
    brush.SetInsets(BorderThickness);
    brush.IsCenterHollow(true);
    brush.Source(m_compositor.CreateColorBrush(winrt::Color{ 255, 255, 0, 0 }));
    m_snipVisual.Brush(brush);
    m_target.Root(m_root);

    m_standardCursor.reset(winrt::check_pointer(LoadCursorW(nullptr, IDC_ARROW)));
    m_crosshairCursor.reset(winrt::check_pointer(LoadCursorW(nullptr, IDC_CROSS)));
    m_cursorType = CursorType::Crosshair;
}

LRESULT MainWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam)
{
    switch (message)
    {
    case WM_SETCURSOR:
        return OnSetCursor();
    case WM_KEYUP:
    {
        auto key = static_cast<uint32_t>(wparam);
        if (key == VK_ESCAPE)
        {
            Hide();
            PostQuitMessage(0);
        }
    }
    break;
    case WM_LBUTTONDOWN:
    {
        auto xPos = GET_X_LPARAM(lparam);
        auto yPos = GET_Y_LPARAM(lparam);
        OnLeftButtonDown(xPos, yPos);
    }
    break;
    case WM_LBUTTONUP:
    {
        auto xPos = GET_X_LPARAM(lparam);
        auto yPos = GET_Y_LPARAM(lparam);
        OnLeftButtonUp(xPos, yPos);
    }
    break;
    case WM_MOUSEMOVE:
    {
        auto xPos = GET_X_LPARAM(lparam);
        auto yPos = GET_Y_LPARAM(lparam);
        OnMouseMove(xPos, yPos);
    }
    break;
    default:
        return base_type::MessageHandler(message, wparam, lparam);
    }
    return 0;
}

void MainWindow::Show()
{
    // Calculate the position and size of the window
    RECT rect = ComputeAllDisplaysUnion();
    m_unionRect = rect;

    ResetSnip();

    auto style = GetWindowLongPtrW(m_window, GWL_EXSTYLE);
    style &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(m_window, GWL_EXSTYLE, style);
    SetWindowPos(
        m_window,
        HWND_TOPMOST,
        m_unionRect.left,
        m_unionRect.top,
        m_unionRect.right - m_unionRect.left,
        m_unionRect.bottom - m_unionRect.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    UpdateWindow(m_window);
}

void MainWindow::Hide()
{
    ShowWindow(m_window, SW_HIDE);
    UpdateWindow(m_window);

    ResetSnip();
}

void MainWindow::ResetSnip()
{
    m_snipStatus = SnipStatus::None;
    m_cursorType = CursorType::Crosshair;

    m_root.Children().RemoveAll();
    m_snipVisual.Size({ 0, 0 });
    m_snipVisual.Offset({ 0, 0, 0 });
}

bool MainWindow::OnSetCursor()
{
    switch (m_cursorType)
    {
    case CursorType::Standard:
        SetCursor(m_standardCursor.get());
        return true;
    case CursorType::Crosshair:
        SetCursor(m_crosshairCursor.get());
        return true;
    default:
        return false;
    }
}

void MainWindow::OnLeftButtonDown(int x, int y)
{
    if (m_snipStatus == SnipStatus::None)
    {
        m_snipStatus = SnipStatus::Ongoing;
        m_snipVisual.Offset({ x - BorderThickness, y - BorderThickness, 0 });
        m_root.Children().InsertAtTop(m_snipVisual);
        m_startPosition = { x, y };
    }
}

void MainWindow::OnLeftButtonUp(int x, int y)
{
    if (m_snipStatus == SnipStatus::Ongoing)
    {
        m_snipStatus = SnipStatus::Completed;
        m_cursorType = CursorType::Standard;

        // Compute our crop rect
        if (x < m_startPosition.x)
        {
            m_snipRect.left = x;
            m_snipRect.right = m_startPosition.x;
        }
        else
        {
            m_snipRect.left = m_startPosition.x;
            m_snipRect.right = x;
        }
        if (y < m_startPosition.y)
        {
            m_snipRect.top = y;
            m_snipRect.bottom = m_startPosition.y;
        }
        else
        {
            m_snipRect.top = m_startPosition.y;
            m_snipRect.bottom = y;
        }

        // Exit if the rect is empty
        if (m_snipRect.right - m_snipRect.left == 0 || m_snipRect.bottom - m_snipRect.top == 0)
        {
            Hide();
            PostQuitMessage(0);
            return;
        }

        // Make the window transparent so we can interact with
        // the rest of the system.
        auto style = GetWindowLongPtrW(m_window, GWL_EXSTYLE);
        style |= WS_EX_TRANSPARENT;
        SetWindowLongPtrW(m_window, GWL_EXSTYLE, style);

        // Tell the user how to proceed
        wprintf(L"Press CTRL+SHIFT+R to start recording...\n");
    }
}

void MainWindow::OnMouseMove(int x, int y)
{
    if (m_snipStatus == SnipStatus::Ongoing)
    {
        auto offset = m_snipVisual.Offset();
        auto size = m_snipVisual.Size();

        if (x < m_startPosition.x)
        {
            offset.x = x - BorderThickness;
            size.x = (m_startPosition.x - x) + (2 * BorderThickness);
        }
        else
        {
            size.x = (x - m_startPosition.x) + (2 * BorderThickness);
        }

        if (y < m_startPosition.y)
        {
            offset.y = y - BorderThickness;
            size.y = (m_startPosition.y - y) + (2 * BorderThickness);
        }
        else
        {
            size.y = (y - m_startPosition.y) + (2 * BorderThickness);
        }

        m_snipVisual.Offset(offset);
        m_snipVisual.Size(size);
    }
}
