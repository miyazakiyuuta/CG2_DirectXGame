#include "scene/TitleScene.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "UI/RuntimeTextTextureGenerator.h"
#include "base/DirectXCommon.h"
#include "base/WinApp.h"
#include "io/Input.h"
#include "scene/SceneManager.h"
#include "transition/BlindTransition.h"

#include <cmath>
#include <memory>
#include <algorithm>

namespace {

	std::string ToUtf8String(const char* text)
	{
		return text ? std::string(text) : std::string();
	}

#if defined(__cpp_char8_t)
	std::string ToUtf8String(const char8_t* text)
	{
		return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
	}
#endif

	float SmoothStep(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}

	struct SkyKeyframe {
		float t;
		Vector4 color;
	};

	Vector4 GetTitleTimeColor(float timer, float cycleSeconds)
	{
		static const SkyKeyframe keys[] = {
			{ 0.00f, { 0.05f, 0.07f, 0.16f, 1.0f } }, // 深夜
			{ 0.10f, { 0.10f, 0.14f, 0.30f, 1.0f } }, // 天文薄明
			{ 0.18f, { 0.20f, 0.28f, 0.46f, 1.0f } }, // 航海薄明
			{ 0.26f, { 0.40f, 0.52f, 0.72f, 1.0f } }, // 市民薄明
			{ 0.34f, { 0.98f, 0.70f, 0.50f, 1.0f } }, // 朝焼け
			{ 0.42f, { 0.78f, 0.90f, 1.00f, 1.0f } }, // 朝
			{ 0.55f, { 0.55f, 0.82f, 1.00f, 1.0f } }, // 昼
			{ 0.68f, { 1.00f, 0.82f, 0.55f, 1.0f } }, // 夕方
			{ 0.76f, { 1.00f, 0.56f, 0.36f, 1.0f } }, // 夕焼け
			{ 0.84f, { 0.54f, 0.34f, 0.54f, 1.0f } }, // 市民薄暮
			{ 0.92f, { 0.18f, 0.22f, 0.40f, 1.0f } }, // 航海薄暮
			{ 1.00f, { 0.05f, 0.07f, 0.16f, 1.0f } }, // 深夜へ戻る
		};

		if (cycleSeconds <= 0.001f) {
			return keys[0].color;
		}

		float loopTime = std::fmod(timer, cycleSeconds);
		if (loopTime < 0.0f) {
			loopTime += cycleSeconds;
		}
		float normalized = loopTime / cycleSeconds;

		for (size_t i = 0; i + 1 < std::size(keys); ++i) {
			if (normalized <= keys[i + 1].t) {
				float localT = (normalized - keys[i].t) / (keys[i + 1].t - keys[i].t);
				localT = SmoothStep(localT);
				return LerpColor(keys[i].color, keys[i + 1].color, localT);
			}
		}

		return keys[std::size(keys) - 1].color;
	}


} // namespace

void TitleScene::Initialize()
{
	input_ = Input::GetInstance();
	spriteCommon_ = SpriteCommon::GetInstance();

	TextureManager::GetInstance()->LoadTexture("white");

	ModelManager::GetInstance()->LoadModel("CubeWhite.obj");
	ModelManager::GetInstance()->LoadModel("Frog", "Frog.gltf");

	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetTranslate({ 0.0f, 6.5f, -32.0f });
	camera_->SetRotate({ 0.10f, 0.0f, 0.0f });

	titleWallColorTimer_ = 0.0f;
	titleWallColorCycleSeconds_ = 60.0f;

	const float wallStartX = -18.0f;
	const float wallStepX = 12.0f;
	const float wallY = 7.5f;
	const float wallZ = 18.0f;


	titleWallObject_ = std::make_unique<Object3d>();
	titleWallObject_->Initialize(Object3dCommon::GetInstance());
	titleWallObject_->SetModel("CubeWhite.obj");
	titleWallObject_->SetCamera(camera_.get());
	titleWallObject_->SetTranslate({
		0.0f,
		0.0f,
		wallZ
		});
	titleWallObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
	titleWallObject_->SetScale({ 40.0f, 20.0f, 1.0f });
	titleWallObject_->SetColor(GetTitleTimeColor(titleWallColorTimer_, titleWallColorCycleSeconds_));
	titleWallObject_->SetEnableLighting(false);

	// 手前の地面
	groundPlaneObject_ = std::make_unique<Object3d>();
	groundPlaneObject_->Initialize(Object3dCommon::GetInstance());
	groundPlaneObject_->SetModel("CubeWhite.obj");
	groundPlaneObject_->SetCamera(camera_.get());
	groundPlaneObject_->SetTranslate({ 0.0f, -9.0f, 6.0f });
	groundPlaneObject_->SetRotate({ 0.0f, 0.0f, 0.0f });
	groundPlaneObject_->SetScale({ 40.0f, 5.0f, 14.0f });
	groundPlaneObject_->SetColor({ 0.10f, 0.22f, 0.18f, 1.0f });

	//Object3dCommon::GetInstance()->SetPointLight({
	//{ 1.0f, 1.0f, 1.0f, 1.0f },
	//{ 0.0f, 8.0f, -20.0f },
	//2.0f,
	//60000.0f,
	//0.0f
	//	});

	// 手前を歩くカエル
	titleFrogObject_ = std::make_unique<Object3d>();
	titleFrogObject_->Initialize(Object3dCommon::GetInstance());
	titleFrogObject_->SetModel("Frog.gltf");
	titleFrogObject_->SetCamera(camera_.get());
	titleFrogObject_->SetTranslate({ frogWalkX_, -1.1f, 5.0f });
	titleFrogObject_->SetRotate({ 0.0f, 1.5707963f, 0.0f });
	titleFrogObject_->SetScale({ 2.2f, 2.2f, 2.2f });
	titleFrogObject_->SetColor({ 0.2f, 0.8f, 0.5f, 1.0f });
	titleFrogObject_->PlayAnimation("walk", true, 0.15f);

	CreatePrimitiveSprite(menuPanelSprite_, { 420.0f, 220.0f }, { 0.02f, 0.05f, 0.08f, 0.72f });
	menuPanelSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	CreatePrimitiveSprite(titleLineSprite_, { 720.0f, 3.0f }, kColorAccent_);
	titleLineSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	CreatePrimitiveSprite(selectorSprite_, { 320.0f, 54.0f }, kColorAccent_);
	selectorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	GenerateTitleTextTextures();

	selectItem_ = MenuItem::GamePlay;
	pulseTimer_ = 0.0f;
}

void TitleScene::Finalize()
{
}

void TitleScene::GenerateTitleTextTextures()
{
	textSprites_.clear();

	CreateTextSprite(
		"title",
		ToUtf8String(u8"跳びざかり"),
		"resources/generated_ui/title_main.png"
	);

	CreateTextSprite(
		"gameplay",
		ToUtf8String(u8"ゲームプレイ"),
		"resources/generated_ui/title_gameplay.png"
	);

	CreateTextSprite(
		"tutorial",
		ToUtf8String(u8"チュートリアル"),
		"resources/generated_ui/title_tutorial.png"
	);

	CreateTextSprite(
		"guide",
		ToUtf8String(u8"W/S または 十字キーで選択  Enter/Aで決定"),
		"resources/generated_ui/title_guide.png"
	);
}

void TitleScene::CreateTextSprite(
	const std::string& key,
	const std::string& textUtf8,
	const std::string& outputPath
) {
	RuntimeTextTextureGenerator::GenerateDesc textDesc;
	textDesc.textUtf8 = textUtf8;
	textDesc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
	textDesc.outputFilePath = outputPath;
	textDesc.fontPixelSize = 96;
	textDesc.paddingX = 48;
	textDesc.paddingY = 28;
	textDesc.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	textDesc.shadowColor = { 0.0f, 0.0f, 0.0f, 0.65f };
	textDesc.shadowOffsetX = 6;
	textDesc.shadowOffsetY = 6;

	if (!RuntimeTextTextureGenerator::GeneratePng(textDesc)) {
		return;
	}

	TextureManager::GetInstance()->LoadTexture(outputPath);

	const DirectX::TexMetadata& meta =
		TextureManager::GetInstance()->GetMetaData(outputPath);

	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon_, outputPath);

	const float drawScale = 0.5f;
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	sprite->SetSize({
		static_cast<float>(meta.width) * drawScale,
		static_cast<float>(meta.height) * drawScale
		});
	sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	sprite->Update();

	textSprites_[key] = std::move(sprite);
}

void TitleScene::CreatePrimitiveSprite(
	std::unique_ptr<Sprite>& sprite,
	const Vector2& size,
	const Vector4& color
) {
	sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon_, "white");
	sprite->SetSize(size);
	sprite->SetColor(color);
	sprite->Update();
}

void TitleScene::Update()
{
	const float deltaTime = 1.0f / 60.0f;
	pulseTimer_ += deltaTime;

	HandleInput();
	UpdateTitleFrog(deltaTime);
	UpdateTitleWallColor(deltaTime);

	if (camera_) {
		camera_->Update();
		camera_->TransferToGPU();
	}

	UpdateSceneObjects();
}

void TitleScene::HandleInput()
{
	const int itemCount = static_cast<int>(MenuItem::Count);
	int index = static_cast<int>(selectItem_);

	const bool up =
		input_->IsTriggerKey(DIK_W) ||
		input_->IsTriggerKey(DIK_UP) ||
		input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_UP);

	const bool down =
		input_->IsTriggerKey(DIK_S) ||
		input_->IsTriggerKey(DIK_DOWN) ||
		input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_DOWN);

	if (up) {
		index = (index + itemCount - 1) % itemCount;
		selectItem_ = static_cast<MenuItem>(index);
	}

	if (down) {
		index = (index + 1) % itemCount;
		selectItem_ = static_cast<MenuItem>(index);
	}

	const bool decide =
		input_->IsTriggerKey(DIK_RETURN) ||
		input_->IsTriggerKey(DIK_SPACE) ||
		input_->IsTriggerPad(XINPUT_GAMEPAD_A);

	if (decide) {
		DecideCurrentItem();
	}
}

void TitleScene::DecideCurrentItem()
{
	if (selectItem_ == MenuItem::GamePlay) {
		SceneManager::GetInstance()->ChangeScene(
			"GAMEPLAY",
			std::make_unique<BlindTransition>()
		);
		return;
	}

	if (selectItem_ == MenuItem::Tutorial) {
		SceneManager::GetInstance()->ChangeScene(
			"TUTORIAL",
			std::make_unique<BlindTransition>()
		);
		return;
	}
}

void TitleScene::UpdateTitleFrog(float deltaTime)
{
	if (frogWalkX_ > frogWrapMaxX_) {
		frogWalkX_ = frogWrapMinX_;
	}

	if (titleFrogObject_) {
		titleFrogObject_->SetTranslate({ frogWalkX_, -1.1f, 5.0f });
		titleFrogObject_->SetRotate({ 0.0f, 1.5707963f, 0.0f });
	}
}

void TitleScene::UpdateTitleWallColor(float deltaTime)
{
	titleWallColorTimer_ += deltaTime;

	if (titleWallObject_) {
		titleWallObject_->SetColor(
			GetTitleTimeColor(titleWallColorTimer_, titleWallColorCycleSeconds_)
		);
	}
}

void TitleScene::UpdateSceneObjects()
{
	if (titleWallObject_) {
		titleWallObject_->Update();
	}

	if (groundPlaneObject_) {
		groundPlaneObject_->Update();
	}

	if (titleFrogObject_) {
		titleFrogObject_->Update();
	}
}

void TitleScene::Draw()
{
	// 3D 背景
	if (titleWallObject_) {
		titleWallObject_->Draw();
	}

	if (groundPlaneObject_) {
		groundPlaneObject_->Draw();
	}

	if (titleFrogObject_) {
		titleFrogObject_->Draw();
	}

	// 2D メニュー
	spriteCommon_->CommonDrawSetting();

	if (menuPanelSprite_) {
		menuPanelSprite_->SetPos({ 960.0f, 470.0f });
		menuPanelSprite_->Update();
		menuPanelSprite_->Draw();
	}

	if (titleLineSprite_) {
		titleLineSprite_->SetPos({ 640.0f, 150.0f });
		titleLineSprite_->SetColor({ kColorAccent_.x, kColorAccent_.y, kColorAccent_.z, 0.55f });
		titleLineSprite_->Update();
		titleLineSprite_->Draw();
	}

	DrawTextSprite(textSprites_["title"].get(), { 640.0f, 95.0f }, kColorAccent_);

	const Vector2 gameplayPos = { 960.0f, 430.0f };
	const Vector2 tutorialPos = { 960.0f, 515.0f };

	Vector2 selectorPos = gameplayPos;
	if (selectItem_ == MenuItem::Tutorial) {
		selectorPos = tutorialPos;
	}

	if (selectorSprite_) {
		const float pulse = (std::sin(pulseTimer_ * 4.0f) * 0.5f + 0.5f) * 0.18f;
		selectorSprite_->SetPos(selectorPos);
		selectorSprite_->SetColor({
			kColorAccent_.x,
			kColorAccent_.y,
			kColorAccent_.z,
			0.18f + pulse
			});
		selectorSprite_->Update();
		selectorSprite_->Draw();
	}

	DrawTextSprite(
		textSprites_["gameplay"].get(),
		gameplayPos,
		selectItem_ == MenuItem::GamePlay ? kColorAccent_ : kColorNormal_
	);

	DrawTextSprite(
		textSprites_["tutorial"].get(),
		tutorialPos,
		selectItem_ == MenuItem::Tutorial ? kColorAccent_ : kColorNormal_
	);

	DrawTextSprite(
		textSprites_["guide"].get(),
		{ 700.0f, 620.0f },
		kColorInactive_
	);
}

void TitleScene::DrawTextSprite(Sprite* sprite, const Vector2& pos, const Vector4& color)
{
	if (!sprite) {
		return;
	}

	sprite->SetPos(pos);
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}