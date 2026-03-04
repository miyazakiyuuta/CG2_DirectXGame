#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "base/WinApp.h"

class Input {
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	static Input* GetInstance();

	void Initialize(WinApp* winApp);
	void Update();

	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber">キー番号( DIK_0 等)</param>
	/// <returns>押されているか</returns>
	bool IsPushKey(BYTE keyNumber);
	/// <summary>
	/// キーのトリガーをチェック
	/// </summary>
	/// <param name="keyNumber">キー番号</param>
	/// <returns>トリガーか</returns>
	bool IsTriggerKey(BYTE keyNumber);

	/// <summary>
	/// マウスボタンの押下をチェック
	/// </summary>
	/// <param name="buttonNumber">0:左, 1:右, 2:中</param>
	/// <returns>押されているか</returns>
	bool IsPressMouse(int buttonNumber);

	/// <summary>
	/// マウスボタンのトリガーをチェック
	/// </summary>
	/// <param name="buttonNumber">0:左, 1:右, 2:中</param>
	/// <returns>今押されたか</returns>
	bool IsTriggerMouse(int buttonNumber);

	/// <summary>
	/// マウスの移動量を取得
	/// </summary>
	/// <returns>x, yの移動量</returns>
	struct MouseMove {
		long x;
		long y;
	};
	MouseMove GetMouseMove();

	/// <summary>
	/// ホイールの回転量を取得
	/// </summary>
	/// <returns>回転量(億ならプラス、手前ならマイナス)</returns>
	long GetMouseWheel();

private:
	static Input* instance_;

	// キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard_;
	// 全キーの入力状態を取得する
	BYTE key_[256] = {};
	// 前回のキーの入力状態
	BYTE keyPre_[256] = {};

	// マウスのデバイス
	ComPtr<IDirectInputDevice8> mouse_;
	DIMOUSESTATE2 mouseState_ = {};
	DIMOUSESTATE2 mouseStatePre_ = {};

	// DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput_;

	// WindowsAPI
	WinApp* winApp_ = nullptr;
};

