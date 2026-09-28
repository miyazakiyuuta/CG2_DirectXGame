#include "scene/TitleScene.h"

#include "base/WinApp.h"
#include "io/Input.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "scene/SceneManager.h"
#include "transition/ShutterTransition.h"
#include "transition/BlindTransition.h"
#include "effect/EffectManager.h"

#include <cmath>
#include <string>

namespace {
	const std::string kTitleTexture = "resources/ui/title.png";
	const std::string kStartTexture = "resources/ui/start.png";
	// 操作説明はゲーム中のHUDと同じ画像を使う(表記が食い違わないように1枚で持つ)
	const std::string kControlsTexture = "resources/ui/controls.png";

	// 画像は1280x720想定で作ってあるので、実解像度に合わせて拡大する。
	// kClientWidthは構成で変わる(Debug=1920x1080 / Release=1280x720)ため直値は使えない
	constexpr float kBaseWidth = 1280.0f;
}

TitleScene::TitleScene() = default;

void TitleScene::Initialize() {
	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kBaseWidth;
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float height = static_cast<float>(WinApp::kClientHeight);

	// Sprite::InitializeはTextureManagerに登録済みのSRVを引くだけなので、先に読み込む
	TextureManager::GetInstance()->LoadTexture(kTitleTexture);
	TextureManager::GetInstance()->LoadTexture(kStartTexture);
	TextureManager::GetInstance()->LoadTexture(kControlsTexture);

	// 上から作品名 → 開始案内 → 操作説明。アンカーを中心にすると画像の実寸に関係なく中央へ揃う
	titleSprite_ = std::make_unique<Sprite>();
	titleSprite_->Initialize(SpriteCommon::GetInstance(), kTitleTexture);
	titleSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	// Initialize直後のサイズ=テクスチャの実寸(AdjustTextureSize)。それを解像度倍率で拡大する
	const Vector2 titleSize = titleSprite_->GetSize();
	titleSprite_->SetSize({ titleSize.x * uiScale, titleSize.y * uiScale });
	titleSprite_->SetPos({ centerX, height * 0.36f });

	startSprite_ = std::make_unique<Sprite>();
	startSprite_->Initialize(SpriteCommon::GetInstance(), kStartTexture);
	startSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 startSize = startSprite_->GetSize();
	startSprite_->SetSize({ startSize.x * uiScale, startSize.y * uiScale });
	startSprite_->SetPos({ centerX, height * 0.62f });

	controlsSprite_ = std::make_unique<Sprite>();
	controlsSprite_->Initialize(SpriteCommon::GetInstance(), kControlsTexture);
	controlsSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 controlsSize = controlsSprite_->GetSize();
	controlsSprite_->SetSize({ controlsSize.x * uiScale, controlsSize.y * uiScale });
	controlsSprite_->SetPos({ centerX, height * 0.84f });
	controlsSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.75f }); // 開始案内より目立たないよう少し透かす(HUDと同じ濃さ)
}

void TitleScene::Finalize() {
}

void TitleScene::Update(float deltaTime) {
	elapsed_ += deltaTime;

	// 開始案内はゆっくり明滅させる(結果画面のリトライ案内と同じ周期)
	const float alpha = 0.55f + 0.45f * std::sin(elapsed_ * 4.0f);
	Vector4 color = startSprite_->GetColor();
	color.w = alpha;
	startSprite_->SetColor(color);

	if (IsStartPressed()) {
		//SceneManager::GetInstance()->ChangeScene("GAMEPLAY", std::make_unique<ShutterTransition>());
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY", std::make_unique<BlindTransition>());
	}

	// 頂点と行列の書き込みはUpdateが行うため、Drawの前に毎フレーム呼ぶ
	titleSprite_->Update();
	startSprite_->Update();
	controlsSprite_->Update();
}

void TitleScene::Draw() {
	// 背景はシーンRTのクリア色(黒)のまま。スプライトは深度OFF・αブレンドON
	SpriteCommon::GetInstance()->CommonDrawSetting();
	titleSprite_->Draw();
	startSprite_->Draw();
	controlsSprite_->Draw();
}

bool TitleScene::IsStartPressed() const {
	Input* input = Input::GetInstance();
	return input->IsTriggerKey(DIK_RETURN) || input->IsTriggerKey(DIK_SPACE) ||
		input->IsTriggerPad(XINPUT_GAMEPAD_A) || input->IsTriggerPad(XINPUT_GAMEPAD_START);
}

TitleScene::~TitleScene() = default;
