#pragma once
#include <Windows.h>
#include <cstdint>

// WindowsAPI
class WinApp {
public: // �ÓI�����o�֐�
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
public: // �����o�֐�
	void Initialize();
	// �I������
	void Finalize();
	// ���b�Z�[�W�̏���
	bool ProcessMessage();

	/* getter */
	HWND GetHwnd() { return hwnd_; }
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

public: // �����o�ϐ�
	// �N���C�A���g�̈�̃T�C�Y
	static const std::int32_t kClientWidth = 1280;
	static const std::int32_t kClientHeight = 720;

private:
	// �E�B���h�E�n���h��
	HWND hwnd_ = nullptr;
	// �E�B���h�E�N���X�̐ݒ�
	WNDCLASS wc_{};
};

