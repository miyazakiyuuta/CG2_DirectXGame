#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "WinApp.h"

class Input {
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:

	void Initialize(WinApp* winApp);
	void Update();

	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber">キー番号( DIK_0 等)</param>
	/// <returns>押されているか</returns>
	bool PushKey(BYTE keyNumber);
	/// <summary>
	/// キーのトリガーをチェック
	/// </summary>
	/// <param name="keyNumber">キー番号</param>
	/// <returns>トリガーか</returns>
	bool TriggerKey(BYTE keyNumber);

private:
	// キーボードのデバイス
	ComPtr<IDirectInputDevice8> keyboard;
	// 全キーの入力状態を取得する
	BYTE key[256] = {};
	// 前回のキーの入力状態
	BYTE keyPre[256] = {};
	// DirectInputのインスタンス
	ComPtr<IDirectInput8> directInput;

	// WindowsAPI
	WinApp* winApp_ = nullptr;
};

