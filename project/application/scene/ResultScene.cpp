#include "scene/ResultScene.h"

#include "base/WinApp.h"
#include "io/Input.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "audio/SoundManager.h"
#include "ui/NumberSprite.h"
#include "scene/SceneManager.h"
#include "transition/BlindTransition.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
	// 選択肢の画像(仮。Pillowで生成した、rank_label.pngと同じ「白文字＋黒縁」)。並びはMenuItemと同じ
	const std::string kMenuTextures[] = { "resources/ui/menu_retry.png", "resources/ui/menu_title.png" };
	// 選択肢を選んだときに移るシーン。並びはMenuItemと同じ
	const char* const kMenuScenes[] = { "GAMEPLAY", "TITLE" };
	constexpr float kMenuHeight = 48.0f;          // 選んでいる選択肢の文字の高さ[px](1280x720基準)
	constexpr float kMenuUnselectedScale = 0.75f; // 選んでいない選択肢の大きさ(選んでいる方に対する割合)
	constexpr float kMenuFadeInDuration = 0.25f;  // 入力の猶予が過ぎてから選択肢が出そろうまで[秒]
	// 左スティックをこれ以上倒したら1つ動かす(倒し始めの小さな揺れでは動かさない)
	constexpr float kStickThreshold = 0.5f;

	// 結果音の音量。どの音を鳴らすかは派生クラスが決める
	constexpr float kResultVolume = 0.8f;

	// スコア表示。数字画像は0〜9が等幅で並んだ1枚
	const std::string kNumberTexture = "resources/ui/numbers.png";
	constexpr int kScoreDigits = 6;
	// 1桁の表示サイズ[px](1280x720基準)。切り出し元セルの比率0.85を保つ
	constexpr float kScoreDigitWidth = 47.0f;
	constexpr float kScoreDigitHeight = 56.0f;
	constexpr float kScoreDigitGap = 0.0f;

	// GamePlaySceneから渡される表示用スコア。
	// ResultSceneは遷移のたびに作り直されるので、実体ではなくここで預かる
	int gPendingScore = 0;

	// 画像は1280x720想定で作ってあるので、実解像度に合わせて拡大する。
	// kClientWidthは構成で変わる(Debug=1920x1080 / Release=1280x720)ため直値は使えない
	constexpr float kBaseWidth = 1280.0f;
}

ResultScene::ResultScene() = default;

void ResultScene::SetScore(int score) {
	gPendingScore = score;
}

float ResultScene::GetUiScale() {
	return static_cast<float>(WinApp::kClientWidth) / kBaseWidth;
}

void ResultScene::Initialize() {
	// 違い(画像・色・音・並び)は仮想関数で派生クラスから引く。
	// コンストラクタではなくInitializeで引くのは、コンストラクタの中で仮想関数を呼ぶと
	// 派生クラスの部分がまだ出来上がっておらず、派生クラスの上書きが効かないため
	const float uiScale = GetUiScale();
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float height = static_cast<float>(WinApp::kClientHeight);

	// 縦の並び(画面の高さに対する割合)。クリアはランクの段が入るぶん、全体を上へ詰める
	const Layout layout = GetLayout();
	const float resultY = height * layout.resultY;
	const float scoreTop = height * layout.scoreTop;
	// 選択肢は縦に2つ並べる(上がRETRY、下がTITLE)
	const float menuTopY = height * layout.menuTopY;
	const float menuSpacing = height * 0.1f;

	// Sprite::InitializeはTextureManagerに登録済みのSRVを引くだけなので、先に読み込む
	const std::string resultTexture = GetResultTexturePath();
	TextureManager::GetInstance()->LoadTexture(resultTexture);

	// 中央に結果、その下に選択肢。アンカーを中心にすると画像の実寸に関係なく中央へ揃う
	resultSprite_ = std::make_unique<Sprite>();
	resultSprite_->Initialize(SpriteCommon::GetInstance(), resultTexture);
	resultSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	// Initialize直後のサイズ=テクスチャの実寸(AdjustTextureSize)。それを解像度倍率で拡大する
	const Vector2 resultSize = resultSprite_->GetSize();
	resultSprite_->SetSize({ resultSize.x * uiScale, resultSize.y * uiScale });
	resultSprite_->SetPos({ centerX, resultY });
	resultSprite_->SetColor(GetResultColor());

	// 選択肢。高さをそろえ、横幅は画像の縦横比から決める(RANKの見出しと同じやり方)
	for (int i = 0; i < kMenuCount; ++i) {
		TextureManager::GetInstance()->LoadTexture(kMenuTextures[i]);
		menuSprites_[i] = std::make_unique<Sprite>();
		menuSprites_[i]->Initialize(SpriteCommon::GetInstance(), kMenuTextures[i]);
		menuSprites_[i]->SetAnchorPoint({ 0.5f, 0.5f });
		const Vector2 textureSize = menuSprites_[i]->GetSize(); // Initialize直後のサイズ=画像の実寸
		const float menuHeight = kMenuHeight * uiScale;
		const float menuWidth = textureSize.y > 0.0f ? menuHeight * textureSize.x / textureSize.y : 0.0f;
		menuSizes_[i] = { menuWidth, menuHeight };
		menuSprites_[i]->SetPos({ centerX, menuTopY + menuSpacing * static_cast<float>(i) });
	}
	selected_ = static_cast<int>(MenuItem::Retry); // 最初はRETRY(CLEAR・GAMEOVERとも同じ)
	UpdateMenu(); // 最初のフレームから、選択肢の大きさと透明度(猶予の間は0)を決めておく

	// 結果音を鳴らす。シーン入場と同時に1回だけ(Updateではなくここ)
	resultSound_ = SoundManager::GetInstance()->LoadFile(GetResultSoundPath());
	const SoundManager::SoundHandle handle = SoundManager::GetInstance()->PlayWave(
		resultSound_, false, SoundManager::SoundCategory::SE);
	SoundManager::GetInstance()->SetVolume(handle, kResultVolume);

	// スコア。結果の文言と操作案内の間に、中央揃えで置く
	const float digitWidth = kScoreDigitWidth * uiScale;
	const float digitGap = kScoreDigitGap * uiScale;
	const float totalWidth = digitWidth * kScoreDigits + digitGap * (kScoreDigits - 1);
	scoreNumber_ = std::make_unique<NumberSprite>();
	scoreNumber_->Initialize(kNumberTexture, kScoreDigits);
	scoreNumber_->SetDigitSize({ digitWidth, kScoreDigitHeight * uiScale }, digitGap);
	scoreNumber_->SetPosition({ centerX - totalWidth * 0.5f, scoreTop });
	scoreNumber_->SetValue(gPendingScore);
}

void ResultScene::Finalize() {
}

void ResultScene::Update(float deltaTime) {
	elapsed_ += deltaTime;

	// 左スティックの傾きを控える(倒した瞬間を、前のフレームとの比較で拾う)
	previousStickY_ = stickY_;
	stickY_ = Input::GetInstance()->GetLeftStickY();

	// 選択肢を出すのは、入力の猶予が過ぎ、かつ遷移の帯が開き切った後。
	// 帯が開いている途中(約1.1秒)はSceneManagerがChangeSceneを無視するので、その間に決定を受け付けると
	// 決定したことになったまま遷移が始まらず、以後の入力も受け付けなくなる(画面から進めなくなる)
	const bool menuReady = elapsed_ >= inputLockDuration_ && !SceneManager::GetInstance()->IsTransitioning();
	if (menuReady) {
		menuTimer_ += deltaTime;
	}
	// 選択肢が出てから、決定するまでの間だけ選択を受け付ける
	if (menuReady && !decided_) {
		// 2つしかないので、端でもう一度押すと反対の端へ回り込む(どちらへ押しても必ず動く)
		if (IsUpPressed()) {
			selected_ = (selected_ + kMenuCount - 1) % kMenuCount;
		}
		if (IsDownPressed()) {
			selected_ = (selected_ + 1) % kMenuCount;
		}
		if (IsDecidePressed()) {
			Decide();
		}
	}
	UpdateMenu();

	// 頂点と行列の書き込みはUpdateが行うため、Drawの前に毎フレーム呼ぶ
	resultSprite_->Update();
	scoreNumber_->Update();
}

void ResultScene::Draw() {
	// 背景はシーンRTのクリア色(黒)のまま。スプライトは深度OFF・αブレンドON
	SpriteCommon::GetInstance()->CommonDrawSetting();
	resultSprite_->Draw();
	for (const std::unique_ptr<Sprite>& menuSprite : menuSprites_) {
		menuSprite->Draw();
	}
	scoreNumber_->Draw(); // 数字画像が未配置なら何も描かない
}

void ResultScene::Decide() {
	decided_ = true;
	// RETRY: 新しいGamePlaySceneが作られるので、HP・敵・弾・進行度はすべて初期状態に戻る。
	// TITLE: タイトルはオープニングから始まる(シーンが作り直されるため)
	SceneManager::GetInstance()->ChangeScene(kMenuScenes[selected_], std::make_unique<BlindTransition>());
}

void ResultScene::UpdateMenu() {
	// 押せる状態になってから、少しかけて出す(押せる状態になったことを画面だけで伝える)。
	// 決定して次の遷移が始まると数えるのは止まるが、出し切った状態のまま残る
	const float appear = std::clamp(menuTimer_ / kMenuFadeInDuration, 0.0f, 1.0f);
	for (int i = 0; i < kMenuCount; ++i) {
		const bool selected = i == selected_;
		// 選んでいる方は白く、ゆっくり脈打たせる(今どちらを選んでいるかと、まだ押せることを伝える)。
		// 決定した後は脈打ちを止めて、選んだものが確定したことを見せる
		// 選んでいない方は小さく暗くして、選んでいる方と並べたときに差がひと目で分かるようにする
		const float pulse = decided_ ? 1.0f : 0.8f + 0.2f * std::sin(elapsed_ * 5.0f);
		const Vector4 color = selected
			? Vector4{ 1.0f, 1.0f, 1.0f, pulse * appear }
			: Vector4{ 0.55f, 0.55f, 0.55f, 0.6f * appear };
		const float scale = selected ? 1.0f : kMenuUnselectedScale;
		menuSprites_[i]->SetColor(color);
		menuSprites_[i]->SetSize({ menuSizes_[i].x * scale, menuSizes_[i].y * scale });
		menuSprites_[i]->Update();
	}
}

bool ResultScene::IsUpPressed() const {
	Input* input = Input::GetInstance();
	const bool stickUp = stickY_ >= kStickThreshold && previousStickY_ < kStickThreshold;
	return input->IsTriggerKey(DIK_UP) || input->IsTriggerKey(DIK_W) ||
		input->IsTriggerPad(XINPUT_GAMEPAD_DPAD_UP) || stickUp;
}

bool ResultScene::IsDownPressed() const {
	Input* input = Input::GetInstance();
	const bool stickDown = stickY_ <= -kStickThreshold && previousStickY_ > -kStickThreshold;
	return input->IsTriggerKey(DIK_DOWN) || input->IsTriggerKey(DIK_S) ||
		input->IsTriggerPad(XINPUT_GAMEPAD_DPAD_DOWN) || stickDown;
}

bool ResultScene::IsDecidePressed() const {
	Input* input = Input::GetInstance();
	return input->IsTriggerKey(DIK_RETURN) || input->IsTriggerKey(DIK_SPACE) ||
		input->IsTriggerPad(XINPUT_GAMEPAD_A) || input->IsTriggerPad(XINPUT_GAMEPAD_START);
}

ResultScene::~ResultScene() = default;
