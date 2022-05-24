#include "pch.h"
#include "FrameCompositor.h"

namespace winrt
{
    using namespace Windows::Graphics;
    using namespace Windows::Graphics::Capture;
}

float CLEARCOLOR[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // RGBA

FrameCompositor::FrameCompositor(
    winrt::com_ptr<ID3D11Device> const& d3dDevice, 
    winrt::com_ptr<ID3D11DeviceContext> const& d3dContext, 
    RECT const& rect)
{
    m_rect = rect;
    winrt::SizeInt32 frameSize = { rect.right - rect.left, rect.bottom - rect.top };

    winrt::com_ptr<ID3D11Texture2D> outputTexture;
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = static_cast<uint32_t>(frameSize.Width);
    textureDesc.Height = static_cast<uint32_t>(frameSize.Height);
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    winrt::check_hresult(d3dDevice->CreateTexture2D(&textureDesc, nullptr, outputTexture.put()));

    winrt::com_ptr<ID3D11RenderTargetView> outputRTV;
    winrt::check_hresult(d3dDevice->CreateRenderTargetView(outputTexture.get(), nullptr, outputRTV.put()));

    m_d3dContext = d3dContext;
    m_outputTexture = outputTexture;
    m_outputRTV = outputRTV;
}

ComposedFrame FrameCompositor::ProcessFrame(winrt::Direct3D11CaptureFrame const& frame)
{
    auto frameTexture = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    auto systemRelativeTime = frame.SystemRelativeTime();
    auto contentSize = frame.ContentSize();

    D3D11_TEXTURE2D_DESC desc = {};
    frameTexture->GetDesc(&desc);

    m_d3dContext->ClearRenderTargetView(m_outputRTV.get(), CLEARCOLOR);

    D3D11_BOX region = {};
    region.left = m_rect.left;
    region.right = m_rect.right;
    region.top = m_rect.top;
    region.bottom = m_rect.bottom;
    region.back = 1;

    m_d3dContext->CopySubresourceRegion(m_outputTexture.get(), 0, 0, 0, 0, frameTexture.get(), 0, &region);

    ComposedFrame composedFrame = {};
    composedFrame.Texture = m_outputTexture;
    composedFrame.SystemRelativeTime = systemRelativeTime;
    return composedFrame;
}

ComposedFrame FrameCompositor::RepeatFrame(winrt::Windows::Foundation::TimeSpan systemRelativeTime)
{
    ComposedFrame composedFrame = {};
    composedFrame.Texture = m_outputTexture;
    composedFrame.SystemRelativeTime = systemRelativeTime;
    return composedFrame;
}
