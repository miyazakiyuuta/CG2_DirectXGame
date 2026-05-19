#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp {
public: // 静的メンバ関数
	static WinApp* GetInstance();

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
public: // メンバ関数
	void Initialize();
	// 終了処理
	void Finalize();
	// メッセージの処理
	bool ProcessMessage();

	/* getter */
	HWND GetHwnd() { return hwnd_; }
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

	static bool IsResized() { return isResized_; }
	static void ClearResizedFlag() { isResized_ = false; }
	static int GetNewWidth() { return newWidth_; }
	static int GetNewHeight() { return newHeight_; }

#ifndef USE_IMGUI
	static bool IsFullscreen() { return isFullscreen_; }
	static void ToggleFullscreen(HWND hwnd);
#endif


public: // メンバ変数
	// クライアント領域のサイズ
#ifdef USE_IMGUI
	static const std::int32_t kClientWidth = 1920;
	static const std::int32_t kClientHeight = 1080;
#else
	static const std::int32_t kClientWidth = 1280;
	static const std::int32_t kClientHeight = 720;
#endif

private:
	static WinApp* instance;
	static bool isResized_;
	static int newWidth_;
	static int newHeight_;

#ifndef USE_IMGUI
	static bool  isFullscreen_;
	static RECT  windowedRect_;
	static DWORD windowedStyle_;
#endif

	// ウィンドウハンドル
	HWND hwnd_ = nullptr;
	// ウィンドウクラスの設定
	WNDCLASS wc_{};
};

