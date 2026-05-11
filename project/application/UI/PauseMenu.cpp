#include "PauseMenu.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "CameraController.h"
#include "base/WinApp.h" // 画面サイズ取得用
#include "io/Input.h"
#include <algorithm>
#include <cmath>

PauseMenu::~PauseMenu() = default;

void PauseMenu::Initialize(SpriteCommon* spriteCommon, CameraController* cameraController) {
	input_ = Input::GetInstance();
	cameraController_ = cameraController;
	spriteCommon_ = spriteCommon;

	auto texManager = TextureManager::GetInstance();
	texManager->LoadTexture("white");

	// 画面全体のサイズを取得
	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	// --- 1. 構造用スプライトの生成 ---
	auto CreatePrim = [&](std::unique_ptr<Sprite>& sprite, const Vector2& size, const Vector4& color) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, "white");
		sprite->SetSize(size);
		sprite->SetColor(color);
	};

	CreatePrim(bgSprite_, {screenW, screenH}, kColorBg);
	CreatePrim(headerLine_, {screenW, 2.0f}, kColorAccent);
	CreatePrim(footerLine_, {screenW, 2.0f}, kColorAccent);
	CreatePrim(tabUnderline_, {240.0f, 4.0f}, kColorAccent);
	CreatePrim(selectorSprite_, {500.0f, 75.0f}, kColorAccent);

	for (int i = 0; i < 2; ++i) {
		CreatePrim(sliderBg_[i], {400.0f, 15.0f}, {0.1f, 0.1f, 0.15f, 1.0f});
		CreatePrim(sliderFill_[i], {400.0f, 15.0f}, kColorAccent);
	}

	// --- 2. テキストスプライトの生成 ---
	std::vector<std::string> paths = {"resources/ui/txt_system.png",  "resources/ui/txt_options.png", "resources/ui/txt_controls.png",    "resources/ui/txt_resume.png",
	                                  "resources/ui/txt_restart.png", "resources/ui/txt_title.png",   "resources/ui/txt_sensitivity.png", "resources/ui/txt_volume.png"};

	for (const auto& path : paths) {
		texManager->LoadTexture(path);
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, path);

		const auto& metadata = texManager->GetMetaData(path);
		Vector2 texSize = {static_cast<float>(metadata.width), static_cast<float>(metadata.height)};

		// UV計算用としてテクスチャ全体のサイズをセット
		sprite->SetTextureSize(texSize);

		// 巨大な画像をUIに適したサイズ（高さ40px）にアスペクト比を維持して縮小表示する
		float targetHeight = 40.0f;
		float targetWidth = texSize.x * (targetHeight / texSize.y);
		sprite->SetSize({targetWidth, targetHeight});

		textSprites_[path] = std::move(sprite);
	}
}

void PauseMenu::Update() {
	if (input_->IsTriggerKey(DIK_ESCAPE)) {
		isPaused_ = !isPaused_;
		if (isPaused_) {
			selectIndex_ = 0;
		}
	}

	if (isPaused_) {
		menuAlpha_ = (std::min)(1.0f, menuAlpha_ + 0.1f);
		pulseTimer_ += 0.05f;
		HandleInput();
	} else {
		menuAlpha_ = (std::max)(0.0f, menuAlpha_ - 0.1f);
	}
}

void PauseMenu::HandleInput() {
	// タブ切り替え
	if (input_->IsTriggerKey(DIK_Q)) {
		activeTab_ = static_cast<Tab>((static_cast<int>(activeTab_) + 2) % 3);
		selectIndex_ = 0;
	}
	if (input_->IsTriggerKey(DIK_E)) {
		activeTab_ = static_cast<Tab>((static_cast<int>(activeTab_) + 1) % 3);
		selectIndex_ = 0;
	}

	int maxItems = (activeTab_ == Tab::System) ? 3 : (activeTab_ == Tab::Options) ? 2 : 0;
	if (maxItems > 0) {
		if (input_->IsTriggerKey(DIK_W))
			selectIndex_ = (selectIndex_ + maxItems - 1) % maxItems;
		if (input_->IsTriggerKey(DIK_S))
			selectIndex_ = (selectIndex_ + 1) % maxItems;
	}

	// 決定処理
	if (input_->IsTriggerKey(DIK_SPACE) || input_->IsTriggerKey(DIK_RETURN)) {
		if (activeTab_ == Tab::System) {
			if (selectIndex_ == 0)
				isPaused_ = false;
			if (selectIndex_ == 1)
				isRestartRequested_ = true;
			if (selectIndex_ == 2)
				isTitleRequested_ = true;
		}
	}

	// オプション調整
	if (activeTab_ == Tab::Options) {
		float delta = 0.0f;
		if (input_->IsPushKey(DIK_A))
			delta = -0.01f;
		if (input_->IsPushKey(DIK_D))
			delta = 0.01f;

		if (selectIndex_ == 0) {
			// 感度倍率を変更 (0.1倍 ～ 5.0倍)
			mouseSensitivity_ = (std::clamp)(mouseSensitivity_ + delta, 0.1f, 5.0f);
			ApplySettings();
		} else if (selectIndex_ == 1) {
			volume_ = (std::clamp)(volume_ + delta, 0.0f, 1.0f);
		}
	}
}

void PauseMenu::ApplySettings() {
	if (cameraController_) {
		// 【修正】マジックナンバーを排除し、コントローラーが保持するベース速度に倍率をかける
		float mult = mouseSensitivity_;

		cameraController_->SetYawSpeed(cameraController_->GetBaseYawSpeed() * mult);
		cameraController_->SetPitchSpeed(cameraController_->GetBasePitchSpeed() * mult);
		cameraController_->SetMouseSensitivity(cameraController_->GetBaseMouseSensitivity() * mult);
	}
}

void PauseMenu::Draw() {
	if (menuAlpha_ <= 0.0f)
		return;

	// パイプライン設定をリセット
	spriteCommon_->CommonDrawSetting();

	// 1. 全画面背景
	bgSprite_->SetColor({kColorBg.x, kColorBg.y, kColorBg.z, kColorBg.w * menuAlpha_});
	bgSprite_->Update();
	bgSprite_->Draw();

	// 2. 装飾線
	Vector4 lineCol = {kColorAccent.x, kColorAccent.y, kColorAccent.z, 0.5f * menuAlpha_};
	headerLine_->SetColor(lineCol);
	headerLine_->SetPos({0, 100});
	headerLine_->Update();
	headerLine_->Draw();

	footerLine_->SetColor(lineCol);
	footerLine_->SetPos({0, 620});
	footerLine_->Update();
	footerLine_->Draw();

	// 3. タブ描画
	DrawTextSprite(textSprites_["resources/ui/txt_system.png"].get(), {350, 45}, (activeTab_ == Tab::System ? kColorAccent : kColorInactive));
	DrawTextSprite(textSprites_["resources/ui/txt_options.png"].get(), {550, 45}, (activeTab_ == Tab::Options ? kColorAccent : kColorInactive));
	DrawTextSprite(textSprites_["resources/ui/txt_controls.png"].get(), {750, 45}, (activeTab_ == Tab::Controls ? kColorAccent : kColorInactive));

	// タブ下線
	float tabX = 350.0f + (static_cast<int>(activeTab_) * 200.0f);
	tabUnderline_->SetPos({tabX, 90});
	tabUnderline_->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, menuAlpha_});
	tabUnderline_->Update();
	tabUnderline_->Draw();

	// 4. コンテンツ描画
	if (activeTab_ == Tab::System) {
		std::string items[] = {"resources/ui/txt_resume.png", "resources/ui/txt_restart.png", "resources/ui/txt_title.png"};
		for (int i = 0; i < 3; ++i) {
			float y = 220.0f + (i * 100.0f);
			if (selectIndex_ == i) {
				float pulse = (std::sin(pulseTimer_ * 4.0f) * 0.5f + 0.5f) * 0.2f;
				selectorSprite_->SetPos({390, y - 10});
				selectorSprite_->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, (0.2f + pulse) * menuAlpha_});
				selectorSprite_->Update();
				selectorSprite_->Draw();
			}
			DrawTextSprite(textSprites_[items[i]].get(), {440, y}, (selectIndex_ == i ? kColorAccent : kColorNormal));
		}
	} else if (activeTab_ == Tab::Options) {
		DrawTextSprite(textSprites_["resources/ui/txt_sensitivity.png"].get(), {200, 250}, (selectIndex_ == 0 ? kColorAccent : kColorNormal));
		DrawTextSprite(textSprites_["resources/ui/txt_volume.png"].get(), {200, 350}, (selectIndex_ == 1 ? kColorAccent : kColorNormal));

		for (int i = 0; i < 2; ++i) {
			float y = 250.0f + (i * 100.0f);
			sliderBg_[i]->SetPos({650, y + 15});
			sliderBg_[i]->SetColor({0.1f, 0.1f, 0.15f, menuAlpha_});
			sliderBg_[i]->Update();
			sliderBg_[i]->Draw();

			float rate = (i == 0) ? (mouseSensitivity_ / 5.0f) : volume_;
			sliderFill_[i]->SetSize({400 * rate, 15});
			sliderFill_[i]->SetPos({650, y + 15});
			sliderFill_[i]->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, menuAlpha_});
			sliderFill_[i]->Update();
			sliderFill_[i]->Draw();
		}
	}
}

void PauseMenu::DrawTextSprite(Sprite* sprite, const Vector2& pos, const Vector4& color) {
	if (!sprite)
		return;
	Vector4 finalCol = color;
	finalCol.w *= menuAlpha_;
	sprite->SetPos(pos);
	sprite->SetColor(finalCol);
	sprite->Update();
	sprite->Draw();
}
