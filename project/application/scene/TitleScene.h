#pragma once

#include "scene/BaseScene.h"
#include "math/Vector2.h"
#include "math/Vector4.h"
#include "audio/SoundManager.h"

#include <array>
#include <map>
#include <memory>
#include <string>

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
	enum class MenuItem {
		GamePlay = 0,
		Tutorial,
		Count,
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

private:
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	MenuItem selectItem_ = MenuItem::GamePlay;

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

};