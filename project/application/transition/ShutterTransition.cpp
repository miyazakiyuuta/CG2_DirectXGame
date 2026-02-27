#include "ShutterTransition.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "2d/Sprite.h"

ShutterTransition::ShutterTransition()
	: shutterHeight_(-720.0f)
	, isClosed_(false)
	, isOpened_(false) {
}

void ShutterTransition::Initialize() {
	//TextureManager::GetInstance()->LoadTexture("resources/white1x1.png");
	//std::string textureHandle = "resources/white1x1.png";
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	std::string textureHandle = "resources/uvChecker.png";
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(SpriteCommon::GetInstance(), textureHandle);
	sprite_->SetPos({ 0.0f,shutterHeight_ });
	sprite_->SetSize({ 1280.0f,720.0f });
	sprite_->SetAnchorPoint({ 0.0f,0.0f });
	sprite_->SetTextureSize({ 512.0f,512.0f });
}

void ShutterTransition::Update() {
	if (!isClosed_) {

		if (shutterHeight_ >= 0.0f) {
			shutterHeight_ = 0.0f;
			isClosed_ = true;
		}
		shutterHeight_ += 10.0f;
	} else if (!isOpened_) {
		shutterHeight_ -= 10.0f;

		if (shutterHeight_ <= -720.0f) {
			shutterHeight_ = -720.0f;
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
