#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include <Xinput.h>
#pragma comment(lib, "xinput.lib")
#include "base/WinApp.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

class Input {
public:
	// namespace省略
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	static Input* GetInstance();
	static void Finalize();

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
	/// <returns>回転量(奥ならプラス、手前ならマイナス)</returns>
	long GetMouseWheel();

public:
	/// <summary>
	/// コントローラーが接続されているか
	/// </summary>
	/// <param name="controllerIndex">コントローラー番号(0〜3)</param>
	bool IsControllerConnected(int controllerIndex = 0) const;

	/// <summary>
	/// ボタンの押下チェック（押し続け）
	/// </summary>
	/// <param name="button">XINPUT_GAMEPAD_A 等</param>
	bool IsPressPad(WORD button, int controllerIndex = 0) const;

	/// <summary>
	/// ボタンのトリガーチェック（押した瞬間）
	/// </summary>
	bool IsTriggerPad(WORD button, int controllerIndex = 0) const;

	/// <summary>
	/// ボタンのリリースチェック（離した瞬間）
	/// </summary>
	bool IsReleasePad(WORD button, int controllerIndex = 0) const;

	/// <summary>
	/// 左スティックの入力値を取得（-1.0f 〜 1.0f）
	/// </summary>
	float GetLeftStickX(int controllerIndex = 0) const;
	float GetLeftStickY(int controllerIndex = 0) const;

	/// <summary>
	/// 右スティックの入力値を取得（-1.0f 〜 1.0f）
	/// </summary>
	float GetRightStickX(int controllerIndex = 0) const;
	float GetRightStickY(int controllerIndex = 0) const;

	/// <summary>
	/// 左トリガーの入力値（0.0f 〜 1.0f）
	/// </summary>
	float GetLeftTrigger(int controllerIndex = 0) const;
	/// <summary>
	/// 右トリガーの入力値（0.0f 〜 1.0f）
	/// </summary>
	float GetRightTrigger(int controllerIndex = 0) const;

	/// <summary>
	/// 振動をセット（0.0f 〜 1.0f）
	/// </summary>
	void SetVibration(float leftMotor, float rightMotor, int controllerIndex = 0);

private:
	static Input* instance_;

	// WindowsAPI
	WinApp* winApp_ = nullptr;

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

	// コントローラー
	static const int kMaxController_ = 4;
	static const int kDeadZone_ = 8000; // スティックのデッドゾーン

	XINPUT_STATE padState_[kMaxController_] = {};
	XINPUT_STATE padStatePre_[kMaxController_] = {};
	bool         padConnected_[kMaxController_] = {};

	// スティックをデッドゾーン処理して -1.0f〜1.0f に変換
	float NormalizeStick(SHORT value) const;

#ifdef USE_IMGUI
	
	ImVec2 sceneImagePos_ = { 0, 0 };
	ImVec2 sceneImageSize_ = { 0, 0 };
	bool sceneViewHovered_ = false;

public:
	void SetSceneViewInfo(const ImVec2& imagePos, const ImVec2& imageSize, bool isHovered);
	/// マウス座標をゲーム解像度(0〜1280, 0〜720)に変換
	/// 戻り値: Sceneビュー内ならtrue、外ならfalse
	bool GetSceneMousePos(float& outX, float& outY) const;

	/// マウスがSceneビュー上にあるか
	bool IsSceneViewHovered() const { return sceneViewHovered_; }

	/// Sceneビュー内イメージのスクリーン座標矩形(ImGuizmo等のオーバーレイ用)
	const ImVec2& GetSceneImagePos() const { return sceneImagePos_; }
	const ImVec2& GetSceneImageSize() const { return sceneImageSize_; }
#endif
};

