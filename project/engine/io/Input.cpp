#include "Input.h"
#include <cassert>
#include <d3d12.h>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

Input* Input::instance_ = nullptr;

Input* Input::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new Input();
	}
	return instance_;
}

void Input::Initialize(WinApp* winApp) {
	winApp_ = winApp;

	HRESULT result;

	// DirectInputの初期化
	//ComPtr<IDirectInput8> directInput = nullptr;
	result = DirectInput8Create(
		winApp_->GetHInstance(),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&directInput_,
		nullptr
	);
	assert(SUCCEEDED(result));
	// キーボードデバイスの生成
	result = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
	assert(SUCCEEDED(result));
	// 入力データ形式のセット
	result = keyboard_->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(result));
	// 排他制御レベルのリセット
	result = keyboard_->SetCooperativeLevel(
		winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY
	);
	assert(SUCCEEDED(result));

	result = directInput_->CreateDevice(GUID_SysMouse, &mouse_, NULL);
	assert(SUCCEEDED(result));

	result = mouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(result));

	result = mouse_->SetCooperativeLevel(
		winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY
	);
	assert(SUCCEEDED(result));
}

void Input::Update() {
	HRESULT result;

	// 前回のキー入力を保存
	memcpy(keyPre_, key_, sizeof(key_));

	// キーボード情報の取得開始
	result = keyboard_->Acquire();
	// 全キーの入力情報を取得する
	result = keyboard_->GetDeviceState(sizeof(key_), key_);

	mouseStatePre_ = mouseState_;

	result = mouse_->Acquire();

	result = mouse_->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState_);

}

bool Input::IsPushKey(BYTE keyNumber) {
	// 指定キーを押していればtrueを返す
	if (key_[keyNumber]) {
		return true;
	}
	return false;
}

bool Input::IsTriggerKey(BYTE keyNumber) {
	if(key_[keyNumber] && !keyPre_[keyNumber]) {
		return true;
	}
	return false;
}

bool Input::IsPressMouse(int buttonNumber) {
	// ボタン番号が範囲内かチェック(DIMOUSESTATE2は8ボタンまで)
	if (buttonNumber < 0 || buttonNumber >= 8)return false;

	// 0x80(128)が立っていれば押されている
	if (mouseState_.rgbButtons[buttonNumber] & 0x80) {
		return true;
	}
	return false;
}

bool Input::IsTriggerMouse(int buttonNumber) {
	if (buttonNumber < 0 || buttonNumber >= 8)return false;

	// 今回押されていて、前回押されていなければトリガー
	if ((mouseState_.rgbButtons[buttonNumber] & 0x80) &&
		!(mouseStatePre_.rgbButtons[buttonNumber] & 0x80)) {
		return true;
	}
	return false;
}

Input::MouseMove Input::GetMouseMove() {
	MouseMove move;
	move.x = mouseState_.lX;
	move.y = mouseState_.lY;
	return move;
}

long Input::GetMouseWheel() {
	return mouseState_.lZ;
}
