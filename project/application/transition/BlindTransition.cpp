#include "BlindTransition.h"
#include "base/WinApp.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "2d/Sprite.h"

BlindTransition::BlindTransition() {
	windowHeight_ = WinApp::kClientHeight;
	isClosed_ = false;
	isOpened_ = false;
}

void BlindTransition::Initialize() {
	TextureManager::GetInstance()->LoadTexture("resources/white2x2.png");
	std::string textureHandle = "resources/white2x2.png";
	
	for (int i = 0; i < kStripCount; ++i) {
		sprites_.emplace_back(std::make_unique<Sprite>());
		sprites_.back()->Initialize(SpriteCommon::GetInstance(), textureHandle);
		sprites_.back()->SetPos({ 0.0f, windowHeight_ / static_cast<float>(kStripCount) * i });
		sprites_.back()->SetSize({ static_cast<float>(WinApp::kClientWidth), windowHeight_ / static_cast<float>(kStripCount) });
		sprites_.back()->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
	}
}

void BlindTransition::Update(float deltaTime) {
	// フレームレートに依存しないよう、実経過時間ぶんだけ進める
	const float fadeStep = fadeSpeed_ * deltaTime;

	if (!isClosed_) {
		timer_ += fadeStep;
		for (int i = 0; i < kStripCount; ++i) {
			// タイマーがi番目の開始タイミングを超えたらフェード開始
			if (timer_ >= i * stripDelay_) {
				Vector4 color = sprites_[i]->GetColor();
				color.w += fadeStep;
				if (color.w >= 1.0f) {
					color.w = 1.0f;
				}
				sprites_[i]->SetColor(color);
			}
		}

		if (sprites_.back()->GetColor().w >= 1.0f) {
			isClosed_ = true;
			timer_ = 0.0f;
		}
	} else if (!isOpened_) {
		timer_ += fadeStep;

		for (int i = kStripCount - 1; i >= 0; --i) {
			int reverseI = kStripCount - 1 - i;
			if (timer_ >= reverseI * stripDelay_) {
				Vector4 color = sprites_[i]->GetColor();
				color.w -= fadeStep;
				if (color.w <= 0.0f) {
					color.w = 0.0f;
				}
				sprites_[i]->SetColor(color);
			}
		}

		if (sprites_.front()->GetColor().w <= 0.0f) {
			isOpened_ = true;
		}
	}

	for (auto& sprite : sprites_) {
		sprite->Update();
	}
}

void BlindTransition::Draw() {
	SpriteCommon::GetInstance()->CommonDrawSetting();
	for (auto& sprite : sprites_) {
		sprite->Draw();
	}
}
