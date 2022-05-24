#include "pch.h"
#include "CaptureGifEncoder.h"

namespace winrt
{
	using namespace Windows::Foundation;
	using namespace Windows::Graphics;
	using namespace Windows::Graphics::Capture;
	using namespace Windows::Graphics::DirectX;
	using namespace Windows::Graphics::DirectX::Direct3D11;
	using namespace Windows::Storage;
	using namespace Windows::Storage::Streams;
}

CaptureGifEncoder::CaptureGifEncoder(winrt::com_ptr<ID3D11Device> const& d3dDevice)
{
	m_d3dDevice = d3dDevice;
}

void CaptureGifEncoder::Start(winrt::GraphicsCaptureItem const& item, RECT const& rect, winrt::StorageFile const& file)
{
	auto lock = m_lock.lock_exclusive();

	if (m_framePool == nullptr)
	{
		auto captureSize = item.Size();
		winrt::com_ptr<ID3D11DeviceContext> d3dContext;
		m_d3dDevice->GetImmediateContext(d3dContext.put());
		auto device = CreateDirect3DDevice(m_d3dDevice.as<IDXGIDevice>().get());
		auto stream = file.OpenAsync(winrt::FileAccessMode::ReadWrite).get();

		// Setup our gif encoder
		m_encoder = std::make_shared<GifEncoder>(m_d3dDevice, d3dContext, stream, rect);

		// Setup Windows.Graphics.Capture
		m_framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
			device,
			winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
			2,
			captureSize);
		m_session = m_framePool.CreateCaptureSession(item);
		m_frameArrivedToken = m_framePool.FrameArrived({ this, &CaptureGifEncoder::OnFrameArrived });
		m_session.StartCapture();
	}
}

void CaptureGifEncoder::Stop()
{
	auto lock = m_lock.lock_exclusive();

	if (m_framePool != nullptr)
	{
		m_framePool.FrameArrived(m_frameArrivedToken);
		m_frameArrivedToken = {};

		m_session.Close();
		m_session = nullptr;
		m_framePool.Close();
		m_framePool = nullptr;
		
		m_encoder->StopEncodingAsync().get();
		m_encoder.reset();
	}
}

void CaptureGifEncoder::OnFrameArrived(winrt::Direct3D11CaptureFramePool const&, winrt::IInspectable const&)
{
	auto lock = m_lock.lock_exclusive();

	if (m_framePool != nullptr)
	{
		auto frame = m_framePool.TryGetNextFrame();
		m_encoder->ProcessFrame(frame);
	}
}
