#include <Windows.h>

// �E�B���h�E�v���V�[�W��
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// m���Z�[�W�ɉ����ăQ�[���ŗL�̏������s��
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

// Windows�A�v���ł̃G���g���[�|�C���g(main�֐�)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	WNDCLASS wc{};
	// �E�B���h�E�v���V�[�W��
	wc.lpfnWndProc = WindowProc;
	// �E�B���h�E�N���X��
	wc.lpszClassName = L"CG2WindowClass";
	// 

	// �o�̓E�B���h�E�ւ̕����o��
	OutputDebugStringA("Hello,DirecctX!\n");

	return 0;
}