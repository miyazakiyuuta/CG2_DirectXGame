#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "math/Vector2.h"
#include "math/Vector4.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "UI/SpriteNumberText.h"

// 前方宣言
class Sprite;
class SpriteCommon;
class Input;
class CameraController;

/// <summary>
/// ゼルダBotW風 2Dポーズメニュー (提供されたSpriteクラスをフル活用)
/// </summary>
class PauseMenu {
public:
	enum class Tab { System, Options, Controls, Count };
	enum class SystemItem { Resume, Restart, Title, Count };
	enum class OptionItem { Sensitivity, Volume, Count };

	PauseMenu() = default;
	~PauseMenu();

	/// <summary>
	/// 初期化：SpriteCommonを受け取り、全てのUIパーツ用スプライトを生成
	/// </summary>
	void Initialize(SpriteCommon* spriteCommon, CameraController* cameraController);

	/// <summary>
	/// 更新：入力処理、アニメーション、および全スプライトの行列更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画：SpriteクラスのDrawを呼び出し
	/// </summary>
	void Draw();

	bool IsPaused() const { return isPaused_; }
	bool IsRestartRequested() const { return isRestartRequested_; }
	bool IsTitleRequested() const { return isTitleRequested_; }

	void ClearRequests() {
		isRestartRequested_ = false;
		isTitleRequested_ = false;
	}
	void SetPaused(bool paused) { isPaused_ = paused; }
	
	// BGM変更時のコールバック登録
	void SetOnBgmChanged(std::function<void(const std::string&)> callback) { onBgmChanged_ = callback; }

private:
	// テキスト画像描画の補助
	void DrawTextSprite(Sprite* sprite, const Vector2& pos, const Vector4& color);

	void HandleInput();
	void ApplySettings();

private:
	Input* input_ = nullptr;
	CameraController* cameraController_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	bool isPaused_ = false;
	bool isRestartRequested_ = false;
	bool isTitleRequested_ = false;

	Tab activeTab_ = Tab::System;
	int selectIndex_ = 0;

	float mouseSensitivity_ = 1.0f;
	float volume_ = 0.5f;

	float menuAlpha_ = 0.0f;
	float pulseTimer_ = 0.0f;

	// --- 構造パーツ用スプライト ---
	std::unique_ptr<Sprite> bgSprite_;
	std::unique_ptr<Sprite> headerLine_;
	std::unique_ptr<Sprite> footerLine_;
	std::unique_ptr<Sprite> tabUnderline_;
	std::unique_ptr<Sprite> selectorSprite_;
	std::unique_ptr<Sprite> sliderBg_[2];
	std::unique_ptr<Sprite> sliderFill_[2];

	// --- テキスト画像用スプライト ---
	std::map<std::string, std::unique_ptr<Sprite>> textSprites_;
	
	// --- BGMリスト用 ---
	std::vector<std::unique_ptr<Sprite>> bgmTextSprites_;
	std::vector<std::string> bgmFilePaths_; // 再生するファイルパスのリスト
	float optionsScrollY_ = 0.0f;
	float targetOptionsScrollY_ = 0.0f;

	// --- 数値表示用 ---
	std::unique_ptr<SpriteNumberText> numTextSensitivity_;
	std::unique_ptr<SpriteNumberText> numTextVolume_;

	// 配色定数
	const Vector4 kColorAccent = {0.0f, 0.9f, 1.0f, 1.0f};   // シアン
	const Vector4 kColorNormal = {1.0f, 1.0f, 1.0f, 1.0f};   // 白
	const Vector4 kColorInactive = {0.4f, 0.4f, 0.4f, 1.0f}; // グレー
	const Vector4 kColorBg = {0.01f, 0.02f, 0.05f, 0.85f};   // 深い紺

	bool isControllerMode_ = false; // 現在コントローラー操作中かどうかのフラグ

	// --- 操作説明用スプライト ---
	std::unique_ptr<Sprite> controlsKB_;
	std::unique_ptr<Sprite> controlsPad_;

	// --- タブ切り替え操作表示用スプライト ---
	std::unique_ptr<Sprite> navLeftKB_;
	std::unique_ptr<Sprite> navRightKB_;
	std::unique_ptr<Sprite> navLeftPad_;
	std::unique_ptr<Sprite> navRightPad_;
	
	std::function<void(const std::string&)> onBgmChanged_;
};