#include "ShutterTransition.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "2d/Sprite.h"
#include "base/WinApp.h"

ShutterTransition::ShutterTransition()
	: shutterHeight_(-static_cast<float>(WinApp::kClientHeight))
	, isClosed_(false)
	, isOpened_(false) {
}

void ShutterTransition::Initialize() {
	TextureManager::GetInstance()->LoadTexture("resources/white2x2.png");
	std::string textureHandle = "resources/white2x2.png";
	//TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	//std::string textureHandle = "resources/uvChecker.png";
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(SpriteCommon::GetInstance(), textureHandle);
	sprite_->SetPos({ 0.0f,shutterHeight_ });
	sprite_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
	sprite_->SetAnchorPoint({ 0.0f,0.0f });
	//sprite_->SetTextureSize({ 512.0f,512.0f });
}

void ShutterTransition::Update(float deltaTime) {
	// 移動速度[px/秒](旧: 10px/フレーム × 60fps)
	const float speed = 600.0f * deltaTime;
	// 全開位置。720の直書きだと解像度変更時に開き切らなくなる
	const float openedHeight = -static_cast<float>(WinApp::kClientHeight);

	if (!isClosed_) {

		if (shutterHeight_ >= 0.0f) {
			shutterHeight_ = 0.0f;
			isClosed_ = true;
		}
		shutterHeight_ += speed;
	} else if (!isOpened_) {
		shutterHeight_ -= speed;

		if (shutterHeight_ <= openedHeight) {
			shutterHeight_ = openedHeight;
			isOpened_ = true;
		}
	}

	sprite_->SetPos({ 0.0f, shutterHeight_ });

	sprite_->Update();
}

void ShutterTransition::Draw() {
	SpriteCommon::GetInstance()->CommonDrawSetting();
	sprite_->Draw();
}
