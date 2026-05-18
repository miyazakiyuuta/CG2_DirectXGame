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
	// タブ名の下に表示するアクセント下線（タブ名に合わせた幅）
	CreatePrim(tabUnderline_, {140.0f, 3.0f}, kColorAccent);
	// 選択中の項目を囲む点滅ハイライト矩形（テキストに合わせた控えめなサイズ）
	CreatePrim(selectorSprite_, {200.0f, 40.0f}, kColorAccent);

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

	// --- 操作説明画像の読み込み ---
	auto LoadControlSprite = [&](std::unique_ptr<Sprite>& sprite, const std::string& path) {
		texManager->LoadTexture(path);
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, path);
		sprite->SetSize({800.0f, 450.0f}); // 画像サイズに合わせて調整してください
		sprite->SetAnchorPoint({0.5f, 0.5f});
		sprite->SetPos({640.0f, 360.0f});
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
		// --- 【修正】入力デバイスの動的検知ロジック ---

		// 1. コントローラー入力を監視
		bool padActive = false;
		if (input_->IsControllerConnected(0)) {
			// いずれかのボタンが押されているか
			if (input_->IsPressPad(0xFFFF, 0))
				padActive = true;
			// スティックが一定以上動いているか（デッドゾーン考慮）
			if (std::abs(input_->GetLeftStickX()) > 0.3f || std::abs(input_->GetLeftStickY()) > 0.3f)
				padActive = true;
			if (std::abs(input_->GetRightStickX()) > 0.3f || std::abs(input_->GetRightStickY()) > 0.3f)
				padActive = true;
			// トリガーが引かれているか
			if (input_->GetLeftTrigger() > 0.2f || input_->GetRightTrigger() > 0.2f)
				padActive = true;
		}

		// 2. キーボード・マウス入力を監視
		bool kbActive = false;
		// 0〜255の全キーを走査して、何かが押されているかチェック
		for (int i = 0; i < 256; ++i) {
			if (input_->IsPushKey(static_cast<BYTE>(i))) {
				kbActive = true;
				break;
			}
		}
		// マウス移動またはクリック
		auto mouseMove = input_->GetMouseMove();
		if (mouseMove.x != 0 || mouseMove.y != 0)
			kbActive = true;
		if (input_->IsPressMouse(0) || input_->IsPressMouse(1))
			kbActive = true;

		// 判定の適用（コントローラー優先、何もなければ維持）
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
	// --- タブ切り替え ---
	// Qキー or LBボタン : 左のタブへ
	if (input_->IsTriggerKey(DIK_Q) ||
	    input_->IsTriggerPad(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
		activeTab_ = static_cast<Tab>((static_cast<int>(activeTab_) + 2) % 3);
		selectIndex_ = 0;
	}
	// Eキー or RBボタン : 右のタブへ
	if (input_->IsTriggerKey(DIK_E) ||
	    input_->IsTriggerPad(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
		activeTab_ = static_cast<Tab>((static_cast<int>(activeTab_) + 1) % 3);
		selectIndex_ = 0;
	}

	// --- カーソル上下移動 ---
	int maxItems = (activeTab_ == Tab::System) ? 3 : (activeTab_ == Tab::Options) ? 2 : 0;
	if (maxItems > 0) {
		// Wキー or Dパッド上 : カーソルを上へ
		if (input_->IsTriggerKey(DIK_W) ||
		    input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_UP))
			selectIndex_ = (selectIndex_ + maxItems - 1) % maxItems;
		// Sキー or Dパッド下 : カーソルを下へ
		if (input_->IsTriggerKey(DIK_S) ||
		    input_->IsTriggerPad(XINPUT_GAMEPAD_DPAD_DOWN))
			selectIndex_ = (selectIndex_ + 1) % maxItems;
	}

	// --- 決定処理 ---
	// SPACEキー / Enterキー or Aボタン
	if (input_->IsTriggerKey(DIK_SPACE) || input_->IsTriggerKey(DIK_RETURN) ||
	    input_->IsTriggerPad(XINPUT_GAMEPAD_A)) {
		if (activeTab_ == Tab::System) {
			if (selectIndex_ == 0)
				isPaused_ = false;
			if (selectIndex_ == 1)
				isRestartRequested_ = true;
			if (selectIndex_ == 2)
				isTitleRequested_ = true;
		}
	}

	// --- キャンセル（ポーズを閉じる） ---
	// Bボタンでポーズメニューを閉じる
	if (input_->IsTriggerPad(XINPUT_GAMEPAD_B)) {
		isPaused_ = false;
	}

	// --- オプション調整（スライダー操作） ---
	if (activeTab_ == Tab::Options) {
		float delta = 0.0f;
		// Aキー or Dパッド左 : 値を下げる
		if (input_->IsPushKey(DIK_A) ||
		    input_->IsPressPad(XINPUT_GAMEPAD_DPAD_LEFT))
			delta = -0.01f;
		// Dキー or Dパッド右 : 値を上げる
		if (input_->IsPushKey(DIK_D) ||
		    input_->IsPressPad(XINPUT_GAMEPAD_DPAD_RIGHT))
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
		// 感度倍率をベース速度にかけて適用
		float mult = mouseSensitivity_;

		// キーボード・マウスの感度
		cameraController_->SetYawSpeed(cameraController_->GetBaseYawSpeed() * mult);
		cameraController_->SetPitchSpeed(cameraController_->GetBasePitchSpeed() * mult);
		cameraController_->SetMouseSensitivity(cameraController_->GetBaseMouseSensitivity() * mult);

		// コントローラーの感度にも同じ倍率を適用
		cameraController_->SetPadYawSpeed(cameraController_->GetBasePadYawSpeed() * mult);
		cameraController_->SetPadPitchSpeed(cameraController_->GetBasePadPitchSpeed() * mult);
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

	// 3. タブ描画 — 3つのタブ名を画面中央に等間隔で並べる
	// 画面幅1280の中央640を基準に、左右に180px間隔で配置
	const float tabCenterX = 640.0f;
	const float tabSpacing = 180.0f;
	const float tabY = 50.0f;
	float tabPositions[3] = {tabCenterX - tabSpacing, tabCenterX, tabCenterX + tabSpacing};

	DrawTextSprite(textSprites_["resources/ui/txt_system.png"].get(), {tabPositions[0], tabY}, (activeTab_ == Tab::System ? kColorAccent : kColorInactive));
	DrawTextSprite(textSprites_["resources/ui/txt_options.png"].get(), {tabPositions[1], tabY}, (activeTab_ == Tab::Options ? kColorAccent : kColorInactive));
	DrawTextSprite(textSprites_["resources/ui/txt_controls.png"].get(), {tabPositions[2], tabY}, (activeTab_ == Tab::Controls ? kColorAccent : kColorInactive));

	// 選択中タブの真下にアクセント下線を描画
	float tabX = tabPositions[static_cast<int>(activeTab_)];
	tabUnderline_->SetPos({tabX, tabY + 40.0f});
	tabUnderline_->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, menuAlpha_});
	tabUnderline_->Update();
	tabUnderline_->Draw();

	// 4. コンテンツ描画
	if (activeTab_ == Tab::System) {
		std::string items[] = {"resources/ui/txt_resume.png", "resources/ui/txt_restart.png", "resources/ui/txt_title.png"};
		// メニュー項目を画面中央に配置（画面幅1280の中央 = 640）
		const float itemCenterX = 640.0f;
		for (int i = 0; i < 3; ++i) {
			float y = 250.0f + (i * 90.0f);
			if (selectIndex_ == i) {
				// 選択中のメニュー項目背景を点滅表示
				float pulse = (std::sin(pulseTimer_ * 4.0f) * 0.5f + 0.5f) * 0.2f;
				// セレクター矩形をテキストの中央に合わせる（200幅の半分=100を引く）
				selectorSprite_->SetPos({itemCenterX - 100.0f, y});
				selectorSprite_->SetColor({kColorAccent.x, kColorAccent.y, kColorAccent.z, (0.2f + pulse) * menuAlpha_});
				selectorSprite_->Update();
				selectorSprite_->Draw();
			}
			// テキストもセレクターと同じ中心に配置
			DrawTextSprite(textSprites_[items[i]].get(), {itemCenterX - 80.0f, y}, (selectIndex_ == i ? kColorAccent : kColorNormal));
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
	} else if (activeTab_ == Tab::Controls) {
			// 現在の入力モードに応じてスプライトを選択
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
