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

	// コントローラーの更新
	for (int i = 0; i < kMaxController_; i++) {
		padStatePre_[i] = padState_[i];

		DWORD padResult = XInputGetState(i, &padState_[i]);
		padConnected_[i] = (padResult == ERROR_SUCCESS);
	}

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

bool Input::IsControllerConnected(int controllerIndex) const {
	if (controllerIndex < 0 || controllerIndex >= kMaxController_) return false;
	return padConnected_[controllerIndex];
}

bool Input::IsPressPad(WORD button, int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return false;
	return (padState_[controllerIndex].Gamepad.wButtons & button) != 0;
}

bool Input::IsTriggerPad(WORD button, int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return false;
	bool now = (padState_[controllerIndex].Gamepad.wButtons & button) != 0;
	bool prev = (padStatePre_[controllerIndex].Gamepad.wButtons & button) != 0;
	return now && !prev;
}

bool Input::IsReleasePad(WORD button, int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return false;
	bool now = (padState_[controllerIndex].Gamepad.wButtons & button) != 0;
	bool prev = (padStatePre_[controllerIndex].Gamepad.wButtons & button) != 0;
	return !now && prev;
}
float Input::GetLeftStickX(int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return 0.0f;
	return NormalizeStick(padState_[controllerIndex].Gamepad.sThumbLX);
}

float Input::GetLeftStickY(int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return 0.0f;
	return NormalizeStick(padState_[controllerIndex].Gamepad.sThumbLY);
}

float Input::GetRightStickX(int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return 0.0f;
	return NormalizeStick(padState_[controllerIndex].Gamepad.sThumbRX);
}

float Input::GetRightStickY(int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return 0.0f;
	return NormalizeStick(padState_[controllerIndex].Gamepad.sThumbRY);
}

float Input::GetLeftTrigger(int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return 0.0f;
	return padState_[controllerIndex].Gamepad.bLeftTrigger / 255.0f;
}

float Input::GetRightTrigger(int controllerIndex) const {
	if (!IsControllerConnected(controllerIndex)) return 0.0f;
	return padState_[controllerIndex].Gamepad.bRightTrigger / 255.0f;
}

void Input::SetVibration(float leftMotor, float rightMotor, int controllerIndex) {
	if (!IsControllerConnected(controllerIndex)) return;

	XINPUT_VIBRATION vibration;
	vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
	vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);
	XInputSetState(controllerIndex, &vibration);
}

float Input::NormalizeStick(SHORT value) const {
	if (value > kDeadZone_)  return static_cast<float>(value - kDeadZone_) / (32767 - kDeadZone_);
	if (value < -kDeadZone_) return static_cast<float>(value + kDeadZone_) / (32767 - kDeadZone_);
	return 0.0f;
}

#ifdef USE_IMGUI
void Input::SetSceneViewInfo(const ImVec2& imagePos, const ImVec2& imageSize, bool isHovered) {
	sceneImagePos_ = imagePos;
	sceneImageSize_ = imageSize;
	sceneViewHovered_ = isHovered;
}

bool Input::GetSceneMousePos(float& outX, float& outY) const {
	if (!sceneViewHovered_) return false;

	ImVec2 mousePos = ImGui::GetMousePos();

	float localX = mousePos.x - sceneImagePos_.x;
	float localY = mousePos.y - sceneImagePos_.y;

	if (localX < 0 || localY < 0 ||
		localX > sceneImageSize_.x ||
		localY > sceneImageSize_.y) {
		return false;
	}

	outX = (localX / sceneImageSize_.x) * 1280.0f;
	outY = (localY / sceneImageSize_.y) * 720.0f;
	return true;
}
#endif