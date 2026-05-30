#pragma once

#include "scene/BaseScene.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "audio/SoundManager.h"

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Sprite;
class SpriteCommon;
class Input;
class Camera;
class Object3d;

class TitleScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	enum class TitleMenuMode {
		Main,
		Difficulty,
	};

	enum class MainMenuItem {
		GamePlay = 0,
		Tutorial,
		Count,
	};

	enum class DifficultyMenuItem {
		Normal = 0,
		Hard,
		Hell,
		Count,
	};

	struct CreditFlowItem {
		std::string groupName;
		std::string texturePath;

		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		float width = 1.0f;
		Vector4 color = { 1.0f, 1.0f, 1.0f, 0.32f };
	};

private:
	void GenerateTitleTextTextures();
	void CreateTextSprite(const std::string& key, const std::string& textUtf8, const std::string& outputPath);
	void CreatePrimitiveSprite(std::unique_ptr<Sprite>& sprite, const Vector2& size, const Vector4& color);

	void HandleInput();
	void DecideCurrentItem();
	void DrawTextSprite(Sprite* sprite, const Vector2& pos, const Vector4& color);

	void UpdateTitleFrog(float deltaTime);
	void UpdateTitleWallColor(float deltaTime);
	void UpdateSceneObjects();
	void UpdateTitleSunMoonPointLight(float deltaTime);

	void GenerateCreditSprites();
	void UpdateCreditFlow(float deltaTime);
	void DrawCreditFlow();
	float GetCreditRespawnX() const;
	float PickCreditRandomY();

private:
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	TitleMenuMode menuMode_ = TitleMenuMode::Main;
	MainMenuItem selectedMainItem_ = MainMenuItem::GamePlay;
	DifficultyMenuItem selectedDifficultyItem_ = DifficultyMenuItem::Normal;

	std::unique_ptr<Camera> camera_ = nullptr;

	std::unique_ptr<Object3d> titleWallObject_ = nullptr;
	std::unique_ptr<Object3d> groundPlaneObject_ = nullptr;
	std::unique_ptr<Object3d> titleFrogObject_ = nullptr;

	float frogWalkX_ = -10.0f;
	float frogWalkSpeed_ = 7.5f;
	float frogWrapMinX_ = -18.0f;
	float frogWrapMaxX_ = 18.0f;
	float pulseTimer_ = 0.0f;

	float titleWallColorTimer_ = 0.55f;
	float titleWallColorCycleSeconds_ = 300.0f;

	std::unique_ptr<Sprite> menuPanelSprite_ = nullptr;
	std::unique_ptr<Sprite> titleLineSprite_ = nullptr;
	std::unique_ptr<Sprite> selectorSprite_ = nullptr;

	std::map<std::string, std::unique_ptr<Sprite>> textSprites_;

	const Vector4 kColorAccent_ = { 0.0f, 0.85f, 1.0f, 1.0f };
	const Vector4 kColorNormal_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	const Vector4 kColorInactive_ = { 0.45f, 0.50f, 0.55f, 1.0f };

	// BGM
	SoundData titleBgm_ = {};
	SoundManager::SoundHandle titleBgmHandle_ = SoundManager::InvalidHandle;
	std::string titleBgmPath_ = "resources/BGM/st005.wav";

	std::vector<CreditFlowItem> creditFlowItems_;

	std::string creditJsonPath_ = "resources/ui/title_credits.json";

	// クレジットの流れる速度は全員共通、ワールド単位/秒
	float creditFlowSpeed_ = 3.8f;

	// 右端外から流し始める位置、ワールドX
	float creditStartX_ = 30.0f;

	// 左に抜けた判定の余白、ワールド単位
	float creditEndMargin_ = 25.0f;

	// 各クレジットの初期X間隔、ワールド単位
	float creditSpawnGapX_ = 17.0f;

	// 初期位置のランダム揺らぎ。速度差ではなく、出現位置差だけ
	float creditInitialJitterX_ = 5.0f;

	// ランダムな高さ範囲、ワールドY
	float creditMinY_ = 0.5f;
	float creditMaxY_ = 13.0f;

	// 文字を置く奥行き
	float creditZ_ = 9.0f;

	// 文字板ポリの高さ、ワールド単位
	float creditWorldHeight_ = 3.45f;

	float creditAlpha_ = 0.32f;
	int creditFontSize_ = 96;

	// 直近のクレジット高さからどのくらい離すか
	float creditMinHeightGapRate_ = 0.18f;

	// ランダム再抽選回数
	int creditHeightPickRetryCount_ = 12;

	// 直近何個分の高さを避けるか
	size_t creditRememberYCount_ = 2;

	// 直近のクレジットY座標
	std::vector<float> recentCreditYs_;

};