#define NOMINMAX
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
#include <fstream>
#include <filesystem>
#include <random>
#include <../externals/nlohmann/json.hpp>
#include "effect/ParticleManager.h"
#include "base/SrvManager.h"
#include "math/Matrix4x4.h"

#include <numbers>

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

	float Saturate(float v)
	{
		return std::clamp(v, 0.0f, 1.0f);
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

	float SmoothStep(float t)
	{
		t = Saturate(t);
		return t * t * (3.0f - 2.0f * t);
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

	float LerpFloat(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	}

	PointLight LerpPointLight(const PointLight& a, const PointLight& b, float t)
	{
		PointLight result{};
		result.color = LerpColor(a.color, b.color, t);
		result.position = LerpVector3(a.position, b.position, t);
		result.intensity = LerpFloat(a.intensity, b.intensity, t);
		result.radius = LerpFloat(a.radius, b.radius, t);
		result.decay = LerpFloat(a.decay, b.decay, t);
		return result;
	}

	float RandomRange(float minV, float maxV)
	{
		static std::mt19937 engine{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(minV, maxV);
		return dist(engine);
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

	ParticleManager::GetInstance()->Initialize(
		DirectXCommon::GetInstance(),
		SrvManager::GetInstance()
	);

	ParticleManager::GetInstance()->SetCamera(camera_.get());

	titleWallColorCycleSeconds_ = 60.0f;
	titleWallColorTimer_ = titleWallColorCycleSeconds_ * 0.42f;

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
	groundPlaneObject_->SetLightIntensity(0.0f);

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
	titleFrogObject_->SetLightIntensity(0.0f);

	CreatePrimitiveSprite(menuPanelSprite_, { 420.0f, 220.0f }, { 0.02f, 0.05f, 0.08f, 0.72f });
	menuPanelSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	CreatePrimitiveSprite(titleLineSprite_, { 720.0f, 3.0f }, kColorAccent_);
	titleLineSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	CreatePrimitiveSprite(selectorSprite_, { 320.0f, 54.0f }, kColorAccent_);
	selectorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	GenerateTitleTextTextures();
	GenerateCreditSprites();

	selectItem_ = MenuItem::GamePlay;
	pulseTimer_ = 0.0f;

	// --- BGMの読み込みとループ再生 ---
	titleBgm_ = SoundManager::GetInstance()->LoadFile(titleBgmPath_);

	titleBgmHandle_ = SoundManager::GetInstance()->PlayWave(
		titleBgm_,
		true,
		SoundManager::SoundCategory::BGM
	);

	// 必要なら音量をここで調整
	SoundManager::GetInstance()->SetCategoryVolume(
		SoundManager::SoundCategory::BGM,
		0.5f
	);
}

void TitleScene::Finalize()
{
	if (titleBgmHandle_ != SoundManager::InvalidHandle) {
		SoundManager::GetInstance()->StopWave(titleBgmHandle_);
		titleBgmHandle_ = SoundManager::InvalidHandle;
	}

	SoundManager::GetInstance()->Unload(titleBgmPath_);
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

	const float drawScale = (key == "title") ? 1.00f : 0.5f;
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
	UpdateTitleSunMoonPointLight(deltaTime);
	UpdateCreditFlow(deltaTime);

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

void TitleScene::UpdateTitleSunMoonPointLight(float deltaTime)
{
	(void)deltaTime;

	if (titleWallColorCycleSeconds_ <= 0.001f) {
		return;
	}

	float loopTime = std::fmod(titleWallColorTimer_, titleWallColorCycleSeconds_);
	if (loopTime < 0.0f) {
		loopTime += titleWallColorCycleSeconds_;
	}

	const float timeRate = loopTime / titleWallColorCycleSeconds_;

	const float sunRiseT = 0.30f;
	const float sunSetT = 0.80f;

	const float moonRiseT = 0.78f;
	const float moonSetT = 0.30f;

	const float blendWidth = 0.06f;

	// 太陽ライトを常に計算
	PointLight sunLight{};
	{
		const float sunT = Saturate((timeRate - sunRiseT) / (sunSetT - sunRiseT));
		const float arc = sunT * 3.14159265f;

		const float x = -18.0f + 36.0f * sunT;
		const float y = 2.5f + std::sin(arc) * 17.0f;
		const float z = -4.0f;

		sunLight.position = { x, y, z };

		const Vector4 sunriseColor = { 1.00f, 0.62f, 0.38f, 1.0f };
		const Vector4 noonColor = { 1.00f, 0.96f, 0.86f, 1.0f };
		const Vector4 sunsetColor = { 1.00f, 0.48f, 0.32f, 1.0f };

		if (sunT < 0.5f) {
			sunLight.color = LerpColor(sunriseColor, noonColor, SmoothStep(sunT * 2.0f));
		}
		else {
			sunLight.color = LerpColor(noonColor, sunsetColor, SmoothStep((sunT - 0.5f) * 2.0f));
		}

		const float noonBlend = std::sin(arc);

		// 朝夕の急な立ち上がりを抑える
		sunLight.intensity = 0.45f + noonBlend * 1.65f;
		sunLight.radius = 115.0f;
		sunLight.decay = 1.15f;
	}

	// 月ライトも常に計算
	PointLight moonLight{};
	{
		float moonT = 0.0f;

		if (timeRate >= moonRiseT) {
			moonT = (timeRate - moonRiseT) / ((1.0f - moonRiseT) + moonSetT);
		}
		else {
			moonT = (timeRate + (1.0f - moonRiseT)) / ((1.0f - moonRiseT) + moonSetT);
		}

		moonT = Saturate(moonT);

		const float arc = moonT * 3.14159265f;

		const float x = 18.0f - 36.0f * moonT;
		const float y = 4.0f + std::sin(arc) * 13.0f;
		const float z = -6.0f;

		moonLight.position = { x, y, z };

		const Vector4 moonLowColor = { 0.25f, 0.32f, 0.62f, 1.0f };
		const Vector4 moonHighColor = { 0.55f, 0.65f, 1.00f, 1.0f };

		const float highBlend = std::sin(arc);
		moonLight.color = LerpColor(moonLowColor, moonHighColor, highBlend);

		// timeRate が 0.0 / 1.0 に近いほど深夜扱い
		float midnightDistance = (std::min)(timeRate, 1.0f - timeRate);
		float midnightDarkRate = 1.0f - SmoothStep(midnightDistance / 0.18f);

		// 深夜だけライトを弱くする
		float nightLightScale = 1.0f - midnightDarkRate * 0.65f;

		moonLight.intensity = (0.10f + highBlend * 0.45f) * nightLightScale;
		moonLight.radius = 75.0f - midnightDarkRate * 20.0f;


		moonLight.decay = 1.3f;
	}

	// 太陽の影響度。日の出前後と日没前後を滑らかにする
	const float sunriseBlend =
		SmoothStep((timeRate - (sunRiseT - blendWidth)) / (blendWidth * 2.0f));

	const float sunsetBlend =
		1.0f - SmoothStep((timeRate - (sunSetT - blendWidth)) / (blendWidth * 2.0f));

	const float sunWeight = Saturate(sunriseBlend * sunsetBlend);

	PointLight finalLight = LerpPointLight(moonLight, sunLight, sunWeight);

	Object3dCommon::GetInstance()->SetPointLight(finalLight);
}

void TitleScene::GenerateCreditSprites()
{

	creditFlowItems_.clear();

	struct LoadedCredit {
		std::string text;
	};

	std::vector<LoadedCredit> credits;

	try {
		if (std::filesystem::exists(creditJsonPath_)) {
			std::ifstream ifs(creditJsonPath_);
			if (ifs.is_open()) {
				nlohmann::json j;
				ifs >> j;

				if (j.contains("credits") && j["credits"].is_array()) {
					for (const auto& item : j["credits"]) {
						std::string text;

						if (item.contains("text") && item["text"].is_string()) {
							text = item["text"].get<std::string>();
						}
						else {
							std::string role;
							std::string name;

							// separator を指定していない場合は「:」を付けない
							std::string separator = "  ";
							bool hasSeparator = false;

							if (item.contains("role") && item["role"].is_string()) {
								role = item["role"].get<std::string>();
							}
							if (item.contains("name") && item["name"].is_string()) {
								name = item["name"].get<std::string>();
							}
							if (item.contains("separator") && item["separator"].is_string()) {
								separator = item["separator"].get<std::string>();
								hasSeparator = true;
							}

							if (!role.empty() && !name.empty()) {
								text = role + separator + name;
							}
							else if (!role.empty()) {
								// role だけの時は、separator指定がある場合だけ後ろに付ける
								text = hasSeparator ? role + separator : role;
							}
							else if (!name.empty()) {
								text = name;
							}
						}

						if (!text.empty()) {
							credits.push_back({ text });
						}
					}
				}
			}
		}
	}
	catch (...) {
		credits.clear();
	}

	if (credits.empty()) {
		credits.push_back({ "Director: Zokusei" });
		credits.push_back({ "Program: Zokusei" });
		credits.push_back({ "Special Thanks: ChatGPT" });
	}

	for (size_t i = 0; i < credits.size(); ++i) {
		const std::string outputPath =
			"resources/generated_ui/title_credit_" + std::to_string(i) + ".png";

		RuntimeTextTextureGenerator::GenerateDesc desc;
		desc.textUtf8 = credits[i].text;
		desc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
		desc.outputFilePath = outputPath;
		desc.overwriteIfExists = true;
		desc.fontPixelSize = creditFontSize_;
		desc.paddingX = 32;
		desc.paddingY = 18;
		desc.textColor = { 0.8f, 0.8f, 0.8f, 1.0f };
		desc.shadowColor = { 0.0f, 0.0f, 0.0f, 0.35f };
		desc.shadowOffsetX = 4;
		desc.shadowOffsetY = 4;

		if (!RuntimeTextTextureGenerator::GeneratePng(desc)) {
			continue;
		}

		TextureManager::GetInstance()->ReloadTexture(outputPath);

		const DirectX::TexMetadata& meta =
			TextureManager::GetInstance()->GetMetaData(outputPath);

		const float imageW = static_cast<float>(meta.width);
		const float imageH = static_cast<float>(meta.height);
		const float aspect = imageH > 0.001f ? imageW / imageH : 1.0f;

		CreditFlowItem flowItem;
		flowItem.groupName = "TitleCredit_" + std::to_string(i);
		flowItem.texturePath = outputPath;

		// 1クレジットにつき1ParticleGroup。
		// 文字ごとにPNGが違うため、グループを分ける。
		ParticleManager::GetInstance()->EnsureParticleGroup(
			flowItem.groupName,
			flowItem.texturePath
		);
		ParticleManager::GetInstance()->SetExternalInstanceCount(flowItem.groupName, 0);

		flowItem.scale = {
			creditWorldHeight_ * aspect,
			creditWorldHeight_,
			1.0f
		};

		flowItem.width = flowItem.scale.x;
		flowItem.color = { 1.0f, 1.0f, 1.0f, creditAlpha_ };

		flowItem.position = {
			creditStartX_ + static_cast<float>(i) * creditSpawnGapX_ +
				RandomRange(0.0f, creditInitialJitterX_),
			PickCreditRandomY(),
			creditZ_
		};

		creditFlowItems_.push_back(std::move(flowItem));
	}

	UpdateCreditFlow(0.0f);
}

float TitleScene::GetCreditRespawnX() const
{
	float rightMostCenter = creditStartX_;

	for (const auto& item : creditFlowItems_) {
		rightMostCenter = std::max(rightMostCenter, item.position.x);
	}

	// 初回配置と同じく「中心座標同士の間隔」を creditSpawnGapX_ にする
	return rightMostCenter + creditSpawnGapX_;
}

float TitleScene::PickCreditRandomY()
{
	const float minY = std::min(creditMinY_, creditMaxY_);
	const float maxY = std::max(creditMinY_, creditMaxY_);
	const float range = maxY - minY;

	if (range <= 0.0001f) {
		recentCreditYs_.push_back(minY);
		if (recentCreditYs_.size() > creditRememberYCount_) {
			recentCreditYs_.erase(recentCreditYs_.begin());
		}
		return minY;
	}

	const float minGap =
		range * std::clamp(creditMinHeightGapRate_, 0.0f, 0.45f);

	auto isFarEnough = [&](float y) {
		for (float recentY : recentCreditYs_) {
			if (std::abs(y - recentY) < minGap) {
				return false;
			}
		}
		return true;
		};

	auto rememberY = [&](float y) {
		recentCreditYs_.push_back(y);

		while (recentCreditYs_.size() > creditRememberYCount_) {
			recentCreditYs_.erase(recentCreditYs_.begin());
		}
		};

	// まずは普通にランダム再抽選
	for (int i = 0; i < creditHeightPickRetryCount_; ++i) {
		float y = RandomRange(minY, maxY);

		if (isFarEnough(y)) {
			rememberY(y);
			return y;
		}
	}

	// どうしても条件を満たせない場合は、
	// 直近2個から一番遠い候補を採用する
	float bestY = RandomRange(minY, maxY);
	float bestScore = -1.0f;

	const int fallbackTryCount = std::max(creditHeightPickRetryCount_ * 2, 16);

	for (int i = 0; i < fallbackTryCount; ++i) {
		float y = RandomRange(minY, maxY);

		float nearestDistance = range;

		for (float recentY : recentCreditYs_) {
			nearestDistance = std::min(nearestDistance, std::abs(y - recentY));
		}

		if (nearestDistance > bestScore) {
			bestScore = nearestDistance;
			bestY = y;
		}
	}

	rememberY(bestY);
	return bestY;
}

void TitleScene::UpdateCreditFlow(float deltaTime)
{
	if (!camera_) {
		return;
	}

	const Matrix4x4& view = camera_->GetViewMatrix();
	const Matrix4x4& projection = camera_->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = view * projection;

	Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();

	Matrix4x4 billboardMatrix = cameraMatrix;
	billboardMatrix.m[3][0] = 0.0f;
	billboardMatrix.m[3][1] = 0.0f;
	billboardMatrix.m[3][2] = 0.0f;

	for (auto& item : creditFlowItems_) {
		item.position.x -= creditFlowSpeed_ * deltaTime;

		if (item.position.x < -item.width * 0.5f - creditEndMargin_) {
			item.position.x = GetCreditRespawnX();
			item.position.y = PickCreditRandomY();
			item.position.z = creditZ_;
		}

		uint32_t maxInstances = 0;
		auto* instancingData =
			ParticleManager::GetInstance()->GetInstancingDataWritePtr(
				item.groupName,
				maxInstances
			);

		if (!instancingData || maxInstances == 0) {
			ParticleManager::GetInstance()->SetExternalInstanceCount(item.groupName, 0);
			continue;
		}

		Matrix4x4 worldMatrix =
			Matrix4x4::Scale(item.scale) *
			billboardMatrix *
			Matrix4x4::Translate(item.position);

		instancingData[0].world = worldMatrix;
		instancingData[0].wvp = worldMatrix * viewProjectionMatrix;
		instancingData[0].color = item.color;

		ParticleManager::GetInstance()->SetExternalInstanceCount(item.groupName, 1);
	}
}

void TitleScene::DrawCreditFlow()
{
	ParticleManager::GetInstance()->Draw();
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

	DrawCreditFlow();

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