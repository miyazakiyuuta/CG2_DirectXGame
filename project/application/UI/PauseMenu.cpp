#include "PauseMenu.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "CameraController.h"
#include "UI/SpriteNumberText.h"
#include "audio/SoundManager.h"
#include "base/WinApp.h"
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

	CreatePrim(tabUnderline_, {140.0f, 3.0f}, kColorAccent);
	tabUnderline_->SetAnchorPoint({0.5f, 0.5f});

	CreatePrim(selectorSprite_, {200.0f, 40.0f}, kColorAccent);
	selectorSprite_->SetAnchorPoint({0.5f, 0.5f});

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

		sprite->SetTextureSize(texSize);

		float targetHeight = 40.0f;
		float targetWidth = texSize.x * (targetHeight / texSize.y);
		sprite->SetSize({targetWidth, targetHeight});

		sprite->SetAnchorPoint({0.5f, 0.5f});

		textSprites_[path] = std::move(sprite);
	}

	// --- 数値テキストの生成 (最大3桁の数値で0〜100を表示) ---
	// 数字用のテクスチャファイル名は環境に合わせて適宜変更してください
	const std::string numberTexturePath = "resources/ui/KiwiMaruNumStrength.png";
	texManager->LoadTexture(numberTexturePath);

	numTextSensitivity_ = std::make_unique<SpriteNumberText>();
	numTextSensitivity_->Initialize(spriteCommon_, numberTexturePath, 3);
	numTextSensitivity_->SetDigitSize({24.0f, 24.0f});

	numTextVolume_ = std::make_unique<SpriteNumberText>();
	numTextVolume_->Initialize(spriteCommon_, numberTexturePath, 3);
	numTextVolume_->SetDigitSize({24.0f, 24.0f});

	// --- 操作説明画像の読み込み ---
	auto LoadControlSprite = [&](std::unique_ptr<Sprite>& sprite, const std::string& path) {
		texManager->LoadTexture(path);
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, path);
		sprite->SetSize({864.0f * 1.5f, 224.0f * 1.5f});
		sprite->SetAnchorPoint({0.5f, 0.5f});
		sprite->SetPos({680.0f, 360.0f});
	};

	LoadControlSprite(controlsKB_, "resources/ui/controls_kb.png");
	LoadControlSprite(controlsPad_, "resources/ui/controls_pad.png");
}

void PauseMenu::Update() {
	if (input_->IsTriggerKey(DIK_ESCAPE) || input_->IsTriggerPad(XINPUT_GAMEPAD_START)) {
		isPaused_ = !isPaused_;
		if (isPaused_)
			selectIndex_ = 0;
	}

	if (isPaused_) {
		bool padActive = false;
		if (input_->IsControllerConnected(0)) {
			if (input_->IsPressPad(0xFFFF, 0))
				padActive = true;
			if (std::abs(input_->GetLeftStickX()) > 0.3f || std::abs(input_->GetLeftStickY()) > 0.3f)
				padActive = true;
			if (std::abs(input_->GetRightStickX()) > 0.3f || std::abs(input_->GetRightStickY()) > 0.3f)
				padActive = true;
			if (input_->GetLeftTrigger() > 0.2f || input_->GetRightTrigger() > 0.2f)
				padActive = true;
		}

		bool kbActive = false;
		for (int i = 0; i < 256; ++i) {
			if (input_->IsPushKey(static_cast<BYTE>(i))) {
				kbActive = true;
				break;
			}
		}
		auto mouseMove = input_->GetMouseMove();
		if (mouseMove.x != 0 || mouseMove.y != 0)
			kbActive = true;
		if (input_->IsPressMouse(0) || input_->IsPressMouse(1))
			kbActive = true;

		if (padActive) {
			isControllerMode_ = true;
		} else if (kbActive) {
			isControllerMode_ = false;
		}

		menuAlpha_ = (std::min)(1.0f, menuAlpha_ + 0.1f);
		pulseTimer_ += 0.05f;
		HandleInput();
	} else {
		menuAlpha_ = (std::max)(0.0f, menuAlpha_ - 0.1f);
	}
}

void PauseMenu::HandleInput() {
	if (input_->IsTriggerKey(DIK_Q) || input_->IsTriggerPad(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
		activeTab_ = static_cast<Tab>((static_cast<int>(activeTab_) + 2) % 3);
		selectIndex_ = 0;
	}
	if (input_->IsTriggerKey(DIK_E) || input_->IsTriggerPad(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
		activeTab_ = static_cast<Tab>((static_cast<int>(activeTab_) + 1) % 3);
		selectIndex_ = 0;
	}

	int maxItems = (activeTab_ == Tab::System) ? 3 : (activeTab_ == Tab::Options) ? 2 : 0;
	if (maxItems > 0) {
		if (input_->IsTriggerKey(DIK_W) || input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_UP))
			selectIndex_ = (selectIndex_ + maxItems - 1) % maxItems;
		if (input_->IsTriggerKey(DIK_S) || input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_DOWN))
			selectIndex_ = (selectIndex_ + 1) % maxItems;
	}

	if (input_->IsTriggerKey(DIK_SPACE) || input_->IsTriggerKey(DIK_RETURN) || input_->IsTriggerPad(XINPUT_GAMEPAD_A)) {
		if (activeTab_ == Tab::System) {
			if (selectIndex_ == 0)
				isPaused_ = false;
			if (selectIndex_ == 1)
				isRestartRequested_ = true;
			if (selectIndex_ == 2)
				isTitleRequested_ = true;
		}
	}

	if (input_->IsTriggerPad(XINPUT_GAMEPAD_B)) {
		isPaused_ = false;
	}

	if (activeTab_ == Tab::Options) {
		float delta = 0.0f;
		if (input_->IsPushKey(DIK_A) || input_->IsPressPad(XINPUT_GAMEPAD_DPAD_LEFT))
			delta = -0.01f;
		if (input_->IsPushKey(DIK_D) || input_->IsPressPad(XINPUT_GAMEPAD_DPAD_RIGHT))
			delta = 0.01f;

		if (selectIndex_ == 0) {
			mouseSensitivity_ = (std::clamp)(mouseSensitivity_ + delta, 0.1f, 5.0f);
			ApplySettings();
		} else if (selectIndex_ == 1) {
			volume_ = (std::clamp)(volume_ + delta, 0.0f, 1.0f);
			ApplySettings();
		}
	}
}

void PauseMenu::ApplySettings() {
	if (cameraController_) {
		float mult = mouseSensitivity_;
		cameraController_->SetYawSpeed(cameraController_->GetBaseYawSpeed() * mult);
		cameraController_->SetPitchSpeed(cameraController_->GetBasePitchSpeed() * mult);
		cameraController_->SetMouseSensitivity(cameraController_->GetBaseMouseSensitivity() * mult);
		cameraController_->SetPadYawSpeed(cameraController_->GetBasePadYawSpeed() * mult);
		cameraController_->SetPadPitchSpeed(cameraController_->GetBasePadPitchSpeed() * mult);
	}
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::BGM, volume_);
}

void PauseMenu::Draw() {
	if (menuAlpha_ <= 0.0f)
		return;

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
	const float tabCenterX = 640.0f;
	const float tabSpacing = 220.0f;
	const float tabY = 50.0f;
	float tabPositions[3] = {tabCenterX - tabSpacing, tabCenterX, tabCenterX + tabSpacing};
	std::string tabKeys[3] = {"resources/ui/txt_system.png", "resources/ui/txt_options.png", "resources/ui/txt_controls.png"};

	for (int i = 0; i < 3; ++i) {
		DrawTextSprite(textSprites_[tabKeys[i]].get(), {tabPositions[i], tabY}, (activeTab_ == static_cast<Tab>(i) ? kColorAccent : kColorInactive));
	}

	auto& activeTabSprite = textSprites_[tabKeys[static_cast<int>(activeTab_)]];
	if (activeTabSprite) {
		float activeTabWidth = activeTabSprite->GetSize().x;
		float tabX = tabPositions[static_cast<int>(activeTab_)];
		tabUnderline_->SetSize({activeTabWidth + 20.0f, 3.0f});
		tabUnderline_->SetPos({tabX, tabY + 25.0f});
		tabUnderline_->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, menuAlpha_});
		tabUnderline_->Update();
		tabUnderline_->Draw();
	}

	// 4. コンテンツ描画
	if (activeTab_ == Tab::System) {
		std::string items[] = {"resources/ui/txt_resume.png", "resources/ui/txt_restart.png", "resources/ui/txt_title.png"};
		const float itemCenterX = 640.0f;
		for (int i = 0; i < 3; ++i) {
			float y = 250.0f + (i * 90.0f);
			if (selectIndex_ == i) {
				float pulse = (std::sin(pulseTimer_ * 4.0f) * 0.5f + 0.5f) * 0.2f;

				auto& currentSprite = textSprites_[items[i]];
				if (currentSprite) {
					float currentWidth = currentSprite->GetSize().x;
					selectorSprite_->SetSize({currentWidth + 60.0f, 50.0f});
				}

				selectorSprite_->SetPos({itemCenterX, y});
				selectorSprite_->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, (0.2f + pulse) * menuAlpha_});
				selectorSprite_->Update();
				selectorSprite_->Draw();
			}
			DrawTextSprite(textSprites_[items[i]].get(), {itemCenterX, y}, (selectIndex_ == i ? kColorAccent : kColorNormal));
		}
	} else if (activeTab_ == Tab::Options) {
		DrawTextSprite(textSprites_["resources/ui/txt_sensitivity.png"].get(), {350.0f, 265.0f}, (selectIndex_ == 0 ? kColorAccent : kColorNormal));
		DrawTextSprite(textSprites_["resources/ui/txt_volume.png"].get(), {350.0f, 365.0f}, (selectIndex_ == 1 ? kColorAccent : kColorNormal));

		for (int i = 0; i < 2; ++i) {
			float y = 250.0f + (i * 100.0f);
			sliderBg_[i]->SetPos({650.0f, y + 15.0f});
			sliderBg_[i]->SetColor({0.1f, 0.1f, 0.15f, menuAlpha_});
			sliderBg_[i]->Update();
			sliderBg_[i]->Draw();

			float rate = (i == 0) ? (mouseSensitivity_ / 5.0f) : volume_;
			sliderFill_[i]->SetSize({400.0f * rate, 15.0f});
			sliderFill_[i]->SetPos({650.0f, y + 15.0f});
			sliderFill_[i]->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, menuAlpha_});
			sliderFill_[i]->Update();
			sliderFill_[i]->Draw();

			// 値を0〜100に変換してテキストで描画
			SpriteNumberText* numText = (i == 0) ? numTextSensitivity_.get() : numTextVolume_.get();
			if (numText) {
				int displayValue = static_cast<int>(rate * 100.0f);
				numText->SetNumber(displayValue, 1);
				// X座標を 1080.0f から 580.0f (メーターの左側) に変更
				numText->SetPosition({580.0f, y});
				numText->SetColor({1.0f, 1.0f, 1.0f, menuAlpha_});
				numText->Update();
				numText->Draw();
			}
		}
	} else if (activeTab_ == Tab::Controls) {
		Sprite* target = isControllerMode_ ? controlsPad_.get() : controlsKB_.get();
		if (target) {
			target->SetColor({1.0f, 1.0f, 1.0f, menuAlpha_});
			target->Update();
			target->Draw();
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