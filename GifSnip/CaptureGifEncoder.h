#pragma once
#include "GifEncoder.h"

class CaptureGifEncoder
{
public:
	CaptureGifEncoder(winrt::com_ptr<ID3D11Device> const& d3dDevice);

	void Start(
		winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item,
		RECT const& rect,
		winrt::Windows::Storage::StorageFile const& file);
	void Stop();

private:
	void OnFrameArrived(
		winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
		winrt::Windows::Foundation::IInspectable const& args);

private:
	winrt::com_ptr<ID3D11Device> m_d3dDevice;
	std::shared_ptr<GifEncoder> m_encoder;
	winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
	winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };
	winrt::event_token m_frameArrivedToken = {};
	wil::srwlock m_lock;
};
