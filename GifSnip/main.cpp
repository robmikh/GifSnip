#include "pch.h"
#include "DisplaysUtil.h"
#include "MainWindow.h"
#include "CaptureGifEncoder.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Storage;
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

winrt::IAsyncOperation<winrt::StorageFile> CreateOutputFile();

int __stdcall wmain(int argc, wchar_t* argv[])
{
    winrt::check_bool(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    // Initialize COM
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    // Create the DispatcherQueue that the compositor needs to run
    auto controller = util::CreateDispatcherQueueControllerForCurrentThread();

    // Create our Compositor
    auto compositor = winrt::Compositor();

    // Initialize D3D
    uint32_t flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    auto d3dDevice = util::CreateD3DDevice(flags);

    // Create our window
    auto window = MainWindow(compositor);

    // Setup hot key
    auto hotKey = util::HotKey(MOD_CONTROL | MOD_SHIFT, 0x52); // R

    // Start and tell the user how to proceed
    window.Show();
    wprintf(L"Drag to select an area of the screen to record...\n");
    auto gifStatus = GifRecordingStatus::None;
    auto encoder = std::make_unique<CaptureGifEncoder>(d3dDevice);

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
                    RECT captureRect = {};
                    if (containingDisplay.has_value())
                    {
                        auto display = containingDisplay.value();
                        item = util::CreateCaptureItemForMonitor(display.Handle());
                        auto displayRect = display.Rect();
                        captureRect = snipRectInDesktopSpace;
                        captureRect.left -= displayRect.left;
                        captureRect.top -= displayRect.top;
                        captureRect.right -= displayRect.left;
                        captureRect.bottom -= displayRect.top;
                    }
                    else
                    {
                        item = util::CreateCaptureItemForMonitor(nullptr);
                        captureRect = snipRect;
                    }

                    // Start gif recording
                    auto file = CreateOutputFile().get();
                    encoder->Start(item, captureRect, file);
                    gifStatus = GifRecordingStatus::Started;
                    wprintf(L"Press CTRL+SHIFT+R to stop recording...\n");
                }
                break;
                case GifRecordingStatus::Started:
                {
                    // Stop gif recording
                    encoder->Stop();
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

winrt::IAsyncOperation<winrt::StorageFile> CreateOutputFile()
{
    auto currentPath = std::filesystem::current_path();
    auto folder = co_await winrt::StorageFolder::GetFolderFromPathAsync(currentPath.wstring());
    auto file = co_await folder.CreateFileAsync(L"recording.gif", winrt::CreationCollisionOption::ReplaceExisting);
    co_return file;
}