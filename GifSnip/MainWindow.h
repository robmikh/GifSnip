#pragma once
#include <robmikh.common/DesktopWindow.h>

enum class SnipStatus
{
	None,
	Ongoing,
	Completed,
};

struct MainWindow : robmikh::common::desktop::DesktopWindow<MainWindow>
{
	static const std::wstring ClassName;
	MainWindow(winrt::Windows::UI::Composition::Compositor const& compositor);
	LRESULT MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);

	void Show();
	void Hide();

	SnipStatus GetSnipStatus() { return m_snipStatus; }
	RECT GetSnipRect() { return m_snipRect; }

private:
	enum class CursorType
	{
		Standard,
		Crosshair,
	};
	
	static const float BorderThickness;
	static void RegisterWindowClass();

	void ResetSnip();

	bool OnSetCursor();
	void OnLeftButtonDown(int x, int y);
	void OnLeftButtonUp(int x, int y);
	void OnMouseMove(int x, int y);

private:
	winrt::Windows::System::DispatcherQueue m_mainThread{ nullptr };
	winrt::Windows::UI::Composition::Compositor m_compositor{ nullptr };
	winrt::Windows::UI::Composition::CompositionTarget m_target{ nullptr };
	winrt::Windows::UI::Composition::SpriteVisual m_root{ nullptr };
	winrt::Windows::UI::Composition::SpriteVisual m_snipVisual{ nullptr };

	RECT m_unionRect = {};
	SnipStatus m_snipStatus = SnipStatus::None;
	POINT m_startPosition = {};
	RECT m_snipRect = {};

	CursorType m_cursorType = CursorType::Standard;
	wil::unique_hcursor m_standardCursor;
	wil::unique_hcursor m_crosshairCursor;
};