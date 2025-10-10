#include "WinApp.h"
#include "externals/imgui/imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
	// ���b�Z�[�W�ɉ����ăQ�[���ŗL�̏������s��
	switch (msg) {
		//�E�B���h�E���j�����ꂽ
	case WM_DESTROY:
		// OS�ɑ΂��āA�A�v���̏I����`����
		PostQuitMessage(0);
		return 0;
	}

	// �W���̃��b�Z�[�W�������s��
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize() {
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// �E�B���h�E�v���V�[�W��
	wc_.lpfnWndProc = WindowProc;
	// �E�B���h�E�N���X��
	wc_.lpszClassName = L"CG2WindowClass";
	// �C���X�^���X�n���h��
	wc_.hInstance = GetModuleHandle(nullptr);
	// �J�[�\��
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// �E�B���h�E�N���X��o�^����
	RegisterClass(&wc_);

	// �E�B���h�E�T�C�Y��\���\���̂ɃN���C�A���g�̈������
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	// �N���C�A���g�̈�����ƂɎ��ۂ̃T�C�Y��wrc��ύX���Ă��炤
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// �E�B���h�E�̐���
	hwnd_ = CreateWindow(
		wc_.lpszClassName,                     // ���p����N���X��
		L"CG2",								  // �^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,				  // �悭����E�B���h�E�X�^�C��
		CW_USEDEFAULT,						  // �\��X���W(Window�ɔC����)
		CW_USEDEFAULT, wrc.right - wrc.left,  // �\��Y���W(WindowOS�ɔC����)
		wrc.bottom - wrc.top,				  // �E�B���h�E����
		nullptr,							  // �E�B���h�E�c��
		nullptr,							  // �e�E�B���h�E�n���h��
		wc_.hInstance,						  // �C���X�^���X�n���h��
		nullptr);							  // �I�v�V����

	// �E�B���h�E��\������
	ShowWindow(hwnd_, SW_SHOW);

}

void WinApp::Finalize() {
	CloseWindow(hwnd_);
	CoUninitialize();
}
bool WinApp::ProcessMessage() {
	MSG msg{};

	// Window�Ƀ��b�Z�[�W�����Ă���ŗD��ŏ���������
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
