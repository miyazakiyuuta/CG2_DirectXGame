#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp {
public: // 静的メンバ関数
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

public: // メンバ変数
	// クライアント領域のサイズ
	static const std::int32_t kClientWidth = 1280;
	static const std::int32_t kClientHeight = 720;

private:
	// ウィンドウハンドル
	HWND hwnd_ = nullptr;
	// ウィンドウクラスの設定
	WNDCLASS wc_{};
};

