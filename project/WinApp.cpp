#include "WinApp.h"
#include "externals/imgui/imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "winmm.lib")


LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
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
		L"LE2B_25_ミヤザキ_ユウタ",			  // タイトルバーの文字
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
;
