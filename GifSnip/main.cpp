#include "pch.h"
#include "DisplaysUtil.h"
#include "MainWindow.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
    using namespace Windows::Graphics::Capture;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::uwp;
}

enum class GifRecordingStatus
{
    None,
    Started,
    Ended,
};

int __stdcall wmain(int argc, wchar_t* argv[])
{
    winrt::check_bool(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    // Initialize COM
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Create the DispatcherQueue that the compositor needs to run
    auto controller = util::CreateDispatcherQueueControllerForCurrentThread();

    // Create our Compositor
    auto compositor = winrt::Compositor();

    // Initialize D3D
    auto d3dDevice = util::CreateD3DDevice();
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    d3dDevice->GetImmediateContext(d3dContext.put());

    // Create our window
    auto window = MainWindow(compositor);

    // Setup hot key
    auto hotKey = util::HotKey(MOD_CONTROL | MOD_SHIFT, 0x52); // R

    // Start and tell the user how to proceed
    window.Show();
    wprintf(L"Drag to select an area of the screen to record...\n");
    auto gifStatus = GifRecordingStatus::None;

    // Message pump
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_HOTKEY)
        {
            if (window.GetSnipStatus() == SnipStatus::Completed)
            {
                switch (gifStatus)
                {
                case GifRecordingStatus::None:
                {
                    // Transform the snip rect into desktop space
                    auto displays = util::DisplayInfo::GetAllDisplays();
                    auto unionRect = ComputeAllDisplaysUnion(displays);
                    auto snipRect = window.GetSnipRect();
                    auto snipRectInDesktopSpace = snipRect;
                    snipRectInDesktopSpace.left += unionRect.left;
                    snipRectInDesktopSpace.top += unionRect.top;
                    snipRectInDesktopSpace.right += unionRect.left;
                    snipRectInDesktopSpace.bottom += unionRect.top;

                    // If our snip rect is wholly within a display rect,
                    // we can capture just that one display instead of 
                    // all displays.
                    std::optional<util::DisplayInfo> containingDisplay = std::nullopt;
                    for (auto&& display : displays)
                    {
                        auto displayRect = display.Rect();
                        if (displayRect.left <= snipRectInDesktopSpace.left &&
                            displayRect.top <= snipRectInDesktopSpace.top &&
                            displayRect.right >= snipRectInDesktopSpace.right &&
                            displayRect.bottom >= snipRectInDesktopSpace.bottom)
                        {
                            containingDisplay = std::optional(display);
                            break;
                        }
                    }

                    winrt::GraphicsCaptureItem item{ nullptr };
                    RECT captureRect = snipRect;
                    if (containingDisplay.has_value())
                    {
                        auto display = containingDisplay.value();
                        item = util::CreateCaptureItemForMonitor(display.Handle());
                        auto displayRect = display.Rect();
                        captureRect.left -= displayRect.left;
                        captureRect.top -= displayRect.top;
                        captureRect.right -= displayRect.left;
                        captureRect.bottom -= displayRect.top;
                    }
                    else
                    {
                        item = util::CreateCaptureItemForMonitor(nullptr);
                    }

                    // TODO: Start gif recording
                    gifStatus = GifRecordingStatus::Started;
                    wprintf(L"Press CTRL+SHIFT+R to stop recording...\n");
                }
                break;
                case GifRecordingStatus::Started:
                {
                    // TODO: Stop gif recording
                    gifStatus = GifRecordingStatus::Ended;
                    wprintf(L"Done!\n");
                    PostQuitMessage(0);
                }
                break;
                default:
                    break;
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return util::ShutdownDispatcherQueueControllerAndWait(controller, static_cast<int>(msg.wParam));
}
