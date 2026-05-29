#include "PauseMenu.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "CameraController.h"
#include "UI/SpriteNumberText.h"
#include "audio/SoundManager.h"
#include "base/WinApp.h"
#include "io/Input.h"
#include "PauseMenu.h"
#include "UI/RuntimeTextTextureGenerator.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include "../../externals/nlohmann/json.hpp"

float PauseMenu::s_mouseSensitivity = 1.0f;
float PauseMenu::s_volume = 0.5f;
std::string PauseMenu::s_currentBgmPath = "resources/BGM/thirdStage.wav";

static void LoadSettings() {
	std::ifstream file("settings.json");
	if (file.is_open()) {
		nlohmann::json j;
		try {
			file >> j;
			if (j.contains("mouseSensitivity")) PauseMenu::s_mouseSensitivity = j["mouseSensitivity"];
			if (j.contains("volume")) PauseMenu::s_volume = j["volume"];
			if (j.contains("currentBgmPath")) PauseMenu::s_currentBgmPath = j["currentBgmPath"];
		} catch (...) {
			// Ignore parse errors
		}
	}
}

static void SaveSettings() {
	std::ofstream file("settings.json");
	if (file.is_open()) {
		nlohmann::json j;
		j["mouseSensitivity"] = PauseMenu::s_mouseSensitivity;
		j["volume"] = PauseMenu::s_volume;
		j["currentBgmPath"] = PauseMenu::s_currentBgmPath;
		file << j.dump(4);
	}
}

PauseMenu::~PauseMenu() = default;

void PauseMenu::Initialize(SpriteCommon* spriteCommon, CameraController* cameraController) {
	static bool s_loaded = false;
	if (!s_loaded) {
		LoadSettings();
		s_loaded = true;
	}

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

	// --- テキスト自動生成関数の一般化 ---
	auto GenAndLoadTextSprite = [&](std::unique_ptr<Sprite>& sprite, const std::string& text, const std::string& path, int fontSize, int padX, int padY) {
		RuntimeTextTextureGenerator::GenerateDesc textDesc;
		textDesc.textUtf8 = text;
		textDesc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
		textDesc.outputFilePath = path;
		textDesc.fontPixelSize = fontSize;
		textDesc.paddingX = padX;
		textDesc.paddingY = padY;
		textDesc.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		textDesc.shadowColor = { 0.0f, 0.0f, 0.0f, 0.65f };
		textDesc.shadowOffsetX = 4;
		textDesc.shadowOffsetY = 4;

		RuntimeTextTextureGenerator::GeneratePng(textDesc);
		texManager->LoadTexture(path);
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, path);
		const auto& meta = texManager->GetMetaData(path);
		Vector2 size = { static_cast<float>(meta.width), static_cast<float>(meta.height) };
		sprite->SetTextureSize(size);
		sprite->SetSize(size);
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
	};

	GenAndLoadTextSprite(navLeftKB_, "Q", "resources/ui/txt_nav_q.png", 22, 8, 4);
	GenAndLoadTextSprite(navRightKB_, "E", "resources/ui/txt_nav_e.png", 22, 8, 4);
	GenAndLoadTextSprite(navLeftPad_, "LB", "resources/ui/txt_nav_lb.png", 22, 8, 4);
	GenAndLoadTextSprite(navRightPad_, "RB", "resources/ui/txt_nav_rb.png", 22, 8, 4);

	// --- BGMリストの生成 ---
	std::vector<std::pair<std::string, std::string>> bgms = {
		{"Dear Childhood Friend", "resources/BGM/Dear Childhood Friend.mp3"},
		{"k012", "resources/BGM/k012.wav"},
		{"ks004", "resources/BGM/ks004.wav"},
		{"thirdStage", "resources/BGM/thirdStage.wav"},
		{"未知の旅へ", "resources/BGM/未知の旅へ.mp3"},
		{"tutorial", "resources/BGM/tutorial.wav"}
	};

	for (size_t i = 0; i < bgms.size(); ++i) {
		std::unique_ptr<Sprite> bgmSprite;
		GenAndLoadTextSprite(bgmSprite, bgms[i].first, "resources/ui/txt_bgm_" + std::to_string(i) + ".png", 32, 10, 5);
		bgmTextSprites_.push_back(std::move(bgmSprite));
		bgmFilePaths_.push_back(bgms[i].second);
	}

	std::unique_ptr<Sprite> bgmTitleSprite;
	GenAndLoadTextSprite(bgmTitleSprite, reinterpret_cast<const char*>(u8"BGM選択"), "resources/ui/txt_bgm_title.png", 32, 10, 5);
	textSprites_["resources/ui/txt_bgm_title.png"] = std::move(bgmTitleSprite);
	
	// 設定を反映
	ApplySettings();
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
		if (std::abs(mouseMove.x) > 5 || std::abs(mouseMove.y) > 5)
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

		optionsScrollY_ += (targetOptionsScrollY_ - optionsScrollY_) * 0.2f;
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

	int maxItems = (activeTab_ == Tab::System) ? 3 : (activeTab_ == Tab::Options) ? (2 + static_cast<int>(bgmFilePaths_.size())) : 0;
	if (maxItems > 0) {
		if (input_->IsTriggerKey(DIK_W) || input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_UP))
			selectIndex_ = (selectIndex_ + maxItems - 1) % maxItems;
		if (input_->IsTriggerKey(DIK_S) || input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_DOWN))
			selectIndex_ = (selectIndex_ + 1) % maxItems;
	}

	// --- マウスによる選択・決定処理 ---
	HWND hwnd = WinApp::GetInstance()->GetHwnd();
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(hwnd, &p);

	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	float clientW = static_cast<float>(clientRect.right - clientRect.left);
	float clientH = static_cast<float>(clientRect.bottom - clientRect.top);
	if (clientW <= 0.0f) clientW = 1280.0f;
	if (clientH <= 0.0f) clientH = 720.0f;

	Vector2 mousePos = { 
		static_cast<float>(p.x) * (1280.0f / clientW), 
		static_cast<float>(p.y) * (720.0f / clientH) 
	};
	
	bool isMouseLeftTriggered = input_->IsTriggerMouse(0);

	if (activeTab_ == Tab::System) {
		for (int i = 0; i < 3; ++i) {
			float y = 250.0f + (i * 90.0f);
			if (mousePos.x > 640.0f - 150.0f && mousePos.x < 640.0f + 150.0f &&
				mousePos.y > y - 40.0f && mousePos.y < y + 40.0f) {
				if (isMouseLeftTriggered) {
					selectIndex_ = i;
					if (i == 0) isPaused_ = false;
					if (i == 1) isRestartRequested_ = true;
					if (i == 2) isTitleRequested_ = true;
				}
			}
		}
	} else if (activeTab_ == Tab::Options) {
		// ホイールによるスクロール（選択カーソルの移動）
		long wheel = input_->GetMouseWheel();
		if (wheel != 0) {
			if (wheel < 0) {
				selectIndex_ = (std::min)(selectIndex_ + 1, maxItems - 1);
			} else {
				selectIndex_ = (std::max)(selectIndex_ - 1, 0);
			}
		}

		// スライダー判定
		for (int i = 0; i < 2; ++i) {
			float y = 250.0f + (i * 100.0f) - optionsScrollY_;
			if (input_->IsPressMouse(0) && mousePos.x >= 650.0f && mousePos.x <= 1050.0f &&
				mousePos.y > y - 40.0f && mousePos.y < y + 40.0f) {
				selectIndex_ = i;
				float rate = (mousePos.x - 650.0f) / 400.0f;
				if (i == 0) { s_mouseSensitivity = (std::clamp)(rate * 5.0f, 0.1f, 5.0f); }
				if (i == 1) { s_volume = (std::clamp)(rate, 0.0f, 1.0f); }
				ApplySettings();
				SaveSettings();
			}
		}
		// BGMリスト判定
		for (int i = 0; i < static_cast<int>(bgmFilePaths_.size()); ++i) {
			float y = 540.0f + (i * 70.0f) - optionsScrollY_;
			int itemIndex = 2 + i;
			if (mousePos.x > 300.0f && mousePos.x < 1000.0f &&
				mousePos.y > y - 35.0f && mousePos.y < y + 35.0f) {
				if (isMouseLeftTriggered) {
					selectIndex_ = itemIndex;
					if (onBgmChanged_) {
						s_currentBgmPath = bgmFilePaths_[i];
						SaveSettings();
						onBgmChanged_(bgmFilePaths_[i]);
					}
				}
			}
		}
	}

	// --- キーボード/パッドによる決定操作 ---
	if (input_->IsTriggerKey(DIK_SPACE) || input_->IsTriggerKey(DIK_RETURN) || input_->IsTriggerPad(XINPUT_GAMEPAD_A)) {
		if (activeTab_ == Tab::System) {
			if (selectIndex_ == 0)
				isPaused_ = false;
			if (selectIndex_ == 1)
				isRestartRequested_ = true;
			if (selectIndex_ == 2)
				isTitleRequested_ = true;
		} else if (activeTab_ == Tab::Options) {
			if (selectIndex_ >= 2) {
				int bgmIndex = selectIndex_ - 2;
				if (onBgmChanged_) {
					s_currentBgmPath = bgmFilePaths_[bgmIndex];
					SaveSettings();
					onBgmChanged_(bgmFilePaths_[bgmIndex]);
				}
			}
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
			s_mouseSensitivity = (std::clamp)(s_mouseSensitivity + delta, 0.1f, 5.0f);
			ApplySettings();
			SaveSettings();
		} else if (selectIndex_ == 1) {
			s_volume = (std::clamp)(s_volume + delta, 0.0f, 1.0f);
			ApplySettings();
			SaveSettings();
		}

		// スクロール計算
		if (selectIndex_ < 2) {
			targetOptionsScrollY_ = 0.0f;
		} else {
			targetOptionsScrollY_ = (selectIndex_ - 1) * 70.0f;
		}
	}
}

void PauseMenu::ApplySettings() {
	if (cameraController_) {
		float mult = s_mouseSensitivity;
		cameraController_->SetYawSpeed(cameraController_->GetBaseYawSpeed() * mult);
		cameraController_->SetPitchSpeed(cameraController_->GetBasePitchSpeed() * mult);
		cameraController_->SetMouseSensitivity(cameraController_->GetBaseMouseSensitivity() * mult);
		cameraController_->SetPadYawSpeed(cameraController_->GetBasePadYawSpeed() * mult);
		cameraController_->SetPadPitchSpeed(cameraController_->GetBasePadPitchSpeed() * mult);
	}
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::BGM, s_volume);
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

	// 3.5 タブ操作用ナビゲーションUIの描画
	Sprite* navL = isControllerMode_ ? navLeftPad_.get() : navLeftKB_.get();
	Sprite* navR = isControllerMode_ ? navRightPad_.get() : navRightKB_.get();

	if (navL) {
		DrawTextSprite(navL, { tabCenterX - tabSpacing - 160.0f, tabY }, kColorAccent);
	}
	if (navR) {
		DrawTextSprite(navR, { tabCenterX + tabSpacing + 160.0f, tabY }, kColorAccent);
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
				selectorSprite_->SetColor({0.0f, 0.4f, 0.9f, (0.4f + pulse) * menuAlpha_});
				selectorSprite_->Update();
				selectorSprite_->Draw();
			}
			DrawTextSprite(textSprites_[items[i]].get(), {itemCenterX, y}, (selectIndex_ == i ? kColorAccent : kColorNormal));
		}
	} else if (activeTab_ == Tab::Options) {
		auto dxCommon = DirectXCommon::GetInstance();

		HWND hwnd = WinApp::GetInstance()->GetHwnd();
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		LONG clientW = clientRect.right - clientRect.left;
		LONG clientH = clientRect.bottom - clientRect.top;

		LONG top = static_cast<LONG>(clientH * (110.0f / 720.0f));
		LONG bottom = static_cast<LONG>(clientH * (610.0f / 720.0f));

		D3D12_RECT scissorRect = { 0, top, clientW, bottom };
		dxCommon->GetCommandList()->RSSetScissorRects(1, &scissorRect);

		DrawTextSprite(textSprites_["resources/ui/txt_sensitivity.png"].get(), {350.0f, 265.0f - optionsScrollY_}, (selectIndex_ == 0 ? kColorAccent : kColorNormal));
		DrawTextSprite(textSprites_["resources/ui/txt_volume.png"].get(), {350.0f, 365.0f - optionsScrollY_}, (selectIndex_ == 1 ? kColorAccent : kColorNormal));

		for (int i = 0; i < 2; ++i) {
			float y = 250.0f + (i * 100.0f) - optionsScrollY_;
			sliderBg_[i]->SetPos({650.0f, y + 15.0f});
			sliderBg_[i]->SetColor({0.1f, 0.1f, 0.15f, menuAlpha_});
			sliderBg_[i]->Update();
			sliderBg_[i]->Draw();

			float rate = (i == 0) ? (s_mouseSensitivity / 5.0f) : s_volume;
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
				numText->SetPosition({580.0f, y});
				numText->SetColor({1.0f, 1.0f, 1.0f, menuAlpha_});
				numText->Update();
				numText->Draw();
			}
		}

		float bgmTitleY = 490.0f - optionsScrollY_;
		DrawTextSprite(textSprites_["resources/ui/txt_bgm_title.png"].get(), {640.0f, bgmTitleY}, kColorNormal);

		// --- BGMリスト描画 ---
		for (size_t i = 0; i < bgmTextSprites_.size(); ++i) {
			float y = 580.0f + (i * 70.0f) - optionsScrollY_;
			int itemIndex = 2 + static_cast<int>(i);

			if (selectIndex_ == itemIndex) {
				float pulse = (std::sin(pulseTimer_ * 4.0f) * 0.5f + 0.5f) * 0.2f;
				if (bgmTextSprites_[i]) {
					float currentWidth = bgmTextSprites_[i]->GetSize().x;
					selectorSprite_->SetSize({currentWidth + 60.0f, 40.0f});
				}
				selectorSprite_->SetPos({640.0f, y});
				selectorSprite_->SetColor({0.0f, 0.4f, 0.9f, (0.4f + pulse) * menuAlpha_});
				selectorSprite_->Update();
				selectorSprite_->Draw();
			}

			DrawTextSprite(bgmTextSprites_[i].get(), {640.0f, y}, (selectIndex_ == itemIndex ? kColorAccent : kColorNormal));
		}

		D3D12_RECT resetRect = { 0, 0, clientW, clientH };
		dxCommon->GetCommandList()->RSSetScissorRects(1, &resetRect);
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
