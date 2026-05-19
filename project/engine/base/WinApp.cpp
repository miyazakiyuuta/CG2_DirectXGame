#include "WinApp.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma comment(lib, "winmm.lib")

WinApp* WinApp::instance = nullptr;

bool WinApp::isResized_ = false;
int WinApp::newWidth_ = 0;
int WinApp::newHeight_ = 0;

#ifndef USE_IMGUI
bool  WinApp::isFullscreen_ = false;
RECT  WinApp::windowedRect_ = {};
DWORD WinApp::windowedStyle_ = 0;
#endif

WinApp* WinApp::GetInstance() {
	if (!instance)instance = new WinApp();
	return instance;
}

LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif
	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (wparam != SIZE_MINIMIZED) {
			newWidth_ = LOWORD(lparam);
			newHeight_ = HIWORD(lparam);
			if (newWidth_ > 0 && newHeight_ > 0) {
				isResized_ = true;
			}
		}
		return 0;
#ifndef USE_IMGUI
	case WM_KEYDOWN:
		if (wparam == VK_F11) {
			ToggleFullscreen(hwnd);
			return 0;
		}
		break;

		// Alt + Enter でもフルスクリーン切り替え
	case WM_SYSKEYDOWN:
		if (wparam == VK_RETURN && (lparam & (1 << 29))) {
			ToggleFullscreen(hwnd);
			return 0;
		}
		break;
#endif
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize() {
	// システムタイマーの分解能を上げる
	timeBeginPeriod(1);

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// ウィンドウプロシージャ
	wc_.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc_.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	RegisterClass(&wc_);

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	// クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,                    // 利用するクラス名
		L"LE3C_21_ミヤザキ_ユウタ",			  // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,				  // よく見るウィンドウスタイル
		CW_USEDEFAULT,						  // 表示X座標(Windowに任せる)
		CW_USEDEFAULT, wrc.right - wrc.left,  // 表示Y座標(WindowOSに任せる)
		wrc.bottom - wrc.top,				  // ウィンドウ横幅
		nullptr,							  // ウィンドウ縦幅
		nullptr,							  // 親ウィンドウハンドル
		wc_.hInstance,						  // インスタンスハンドル
		nullptr);							  // オプション

	// ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);

}

void WinApp::Finalize() {
	CloseWindow(hwnd_);
	CoUninitialize();
}
bool WinApp::ProcessMessage() {
	MSG msg{};

	// Windowにメッセージが来てたら最優先で処理させる
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if(msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

#ifndef USE_IMGUI
void WinApp::ToggleFullscreen(HWND hwnd) {
	if (!isFullscreen_) {
		// --- ウィンドウ状態を保存 ---
		windowedStyle_ = static_cast<DWORD>(GetWindowLong(hwnd, GWL_STYLE));
		GetWindowRect(hwnd, &windowedRect_);

		// --- モニター全体のサイズを取得 ---
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);

		// --- ボーダーレスで全画面に ---
		SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(hwnd, HWND_TOPMOST,
			mi.rcMonitor.left,
			mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		newWidth_ = mi.rcMonitor.right - mi.rcMonitor.left;
		newHeight_ = mi.rcMonitor.bottom - mi.rcMonitor.top;
		isResized_ = true;
		isFullscreen_ = true;
	} else {
		// --- ウィンドウモードに戻す ---
		SetWindowLong(hwnd, GWL_STYLE, static_cast<LONG>(windowedStyle_));
		SetWindowPos(hwnd, HWND_NOTOPMOST,
			windowedRect_.left,
			windowedRect_.top,
			windowedRect_.right - windowedRect_.left,
			windowedRect_.bottom - windowedRect_.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE);

		// クライアント領域サイズをスワップチェーンに反映させる
		RECT clientRect{};
		GetClientRect(hwnd, &clientRect);
		newWidth_ = clientRect.right - clientRect.left;
		newHeight_ = clientRect.bottom - clientRect.top;
		isResized_ = true;
		isFullscreen_ = false;
	}
}
#endif