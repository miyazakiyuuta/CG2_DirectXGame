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
	const std::string kClearTexture = "resources/ui/clear.png";
	const std::string kGameOverTexture = "resources/ui/gameover.png";
	const std::string kRetryTexture = "resources/ui/retry.png";

	// 結果音。表示と同時に鳴らす
	const std::string kClearSound = "resources/sounds/clear.mp3";
	const std::string kGameOverSound = "resources/sounds/gameover.mp3";
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
	// 同じく、表示用のランク(CLEARのときだけ使う)
	StageProgress::Rank gPendingRank = StageProgress::Rank::C;

	// ランク(仮の画像。Pythonで生成したもの。clear.pngと同じ「白文字＋黒縁」)。
	// ranks.png は S/A/B/C を左から等幅に並べてあり、Rankの並び(S=0〜C=3)の位置を切り出す
	const std::string kRankTexture = "resources/ui/ranks.png";
	const std::string kRankLabelTexture = "resources/ui/rank_label.png";
	constexpr int kRankCount = 4;
	constexpr float kRankLetterHeight = 110.0f; // 文字の高さ[px](1280x720基準)。結果の中で一番目立たせる
	constexpr float kRankLabelHeight = 40.0f;   // 「RANK」の高さ[px]
	constexpr float kRankGap = 16.0f;           // 「RANK」と文字の間[px]
	// ランクごとの色(画像は白文字なので、色を掛けると文字だけが染まる)。
	// Sは区間のPERFECTと同じ金色にして「全部うまくいった」ことを同じ色で伝える
	Vector4 RankColor(StageProgress::Rank rank) {
		switch (rank) {
		case StageProgress::Rank::S: return { 1.0f, 0.85f, 0.2f, 1.0f };
		case StageProgress::Rank::A: return { 1.0f, 0.55f, 0.3f, 1.0f };
		case StageProgress::Rank::B: return { 0.55f, 0.85f, 1.0f, 1.0f };
		default:                     return { 0.75f, 0.75f, 0.75f, 1.0f };
		}
	}

	// 画像は1280x720想定で作ってあるので、実解像度に合わせて拡大する。
	// kClientWidthは構成で変わる(Debug=1920x1080 / Release=1280x720)ため直値は使えない
	constexpr float kBaseWidth = 1280.0f;
}

ResultScene::ResultScene(Type type) : type_(type) {}

void ResultScene::SetScore(int score) {
	gPendingScore = score;
}

void ResultScene::SetRank(StageProgress::Rank rank) {
	gPendingRank = rank;
}

void ResultScene::Initialize() {
	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kBaseWidth;
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float height = static_cast<float>(WinApp::kClientHeight);

	// 縦の並び(画面の高さに対する割合)。CLEARはランクの段が入るぶん、全体を上へ詰める。
	// ゲームオーバーはランクを出さないので従来どおり
	const bool isClear = type_ == Type::Clear;
	const float resultY = height * (isClear ? 0.28f : 0.42f);
	const float rankY = height * 0.45f;
	const float scoreTop = height * (isClear ? 0.57f : 0.55f);
	const float retryY = height * (isClear ? 0.74f : 0.68f);

	// Sprite::InitializeはTextureManagerに登録済みのSRVを引くだけなので、先に読み込む
	const std::string& resultTexture = (type_ == Type::Clear) ? kClearTexture : kGameOverTexture;
	TextureManager::GetInstance()->LoadTexture(resultTexture);
	TextureManager::GetInstance()->LoadTexture(kRetryTexture);

	// 中央に結果、その下に操作案内。アンカーを中心にすると画像の実寸に関係なく中央へ揃う
	resultSprite_ = std::make_unique<Sprite>();
	resultSprite_->Initialize(SpriteCommon::GetInstance(), resultTexture);
	resultSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	// Initialize直後のサイズ=テクスチャの実寸(AdjustTextureSize)。それを解像度倍率で拡大する
	const Vector2 resultSize = resultSprite_->GetSize();
	resultSprite_->SetSize({ resultSize.x * uiScale, resultSize.y * uiScale });
	resultSprite_->SetPos({ centerX, resultY });
	// クリアは白、ゲームオーバーは赤みを足して一目で区別できるようにする
	resultSprite_->SetColor((type_ == Type::Clear)
		? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
		: Vector4{ 1.0f, 0.45f, 0.45f, 1.0f });

	retrySprite_ = std::make_unique<Sprite>();
	retrySprite_->Initialize(SpriteCommon::GetInstance(), kRetryTexture);
	retrySprite_->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 retrySize = retrySprite_->GetSize();
	retrySprite_->SetSize({ retrySize.x * uiScale, retrySize.y * uiScale });
	retrySprite_->SetPos({ centerX, retryY });

	// 結果音を鳴らす。シーン入場と同時に1回だけ(Updateではなくここ)
	const std::string& soundPath = (type_ == Type::Clear) ? kClearSound : kGameOverSound;
	resultSound_ = SoundManager::GetInstance()->LoadFile(soundPath);
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

	// ランク(駆除報告)。CLEARのときだけ、「RANK」と文字を横に並べて中央に置く
	if (isClear) {
		TextureManager::GetInstance()->LoadTexture(kRankTexture);
		TextureManager::GetInstance()->LoadTexture(kRankLabelTexture);

		rankLabelSprite_ = std::make_unique<Sprite>();
		rankLabelSprite_->Initialize(SpriteCommon::GetInstance(), kRankLabelTexture);
		rankLabelSprite_->SetAnchorPoint({ 0.5f, 0.5f });
		// Initialize直後のサイズ=画像の実寸。縦横比を保って、高さだけ決める
		const Vector2 labelTexture = rankLabelSprite_->GetSize();
		const float labelHeight = kRankLabelHeight * uiScale;
		const float labelWidth = labelTexture.y > 0.0f ? labelHeight * labelTexture.x / labelTexture.y : 0.0f;

		rankSprite_ = std::make_unique<Sprite>();
		rankSprite_->Initialize(SpriteCommon::GetInstance(), kRankTexture);
		rankSprite_->SetAnchorPoint({ 0.5f, 0.5f });
		// 1文字分の枠 = 画像の横幅 ÷ 4。Rankの並び(S=0〜C=3)の番号の枠を切り出す
		const Vector2 rankTexture = rankSprite_->GetSize();
		const float cellWidth = rankTexture.x / static_cast<float>(kRankCount);
		const int rankIndex = std::clamp(static_cast<int>(gPendingRank), 0, kRankCount - 1);
		rankSprite_->SetTextureLeftTop({ cellWidth * static_cast<float>(rankIndex), 0.0f });
		rankSprite_->SetTextureSize({ cellWidth, rankTexture.y });
		const float letterHeight = kRankLetterHeight * uiScale;
		const float letterWidth = rankTexture.y > 0.0f ? letterHeight * cellWidth / rankTexture.y : 0.0f;

		// 「RANK」+隙間+文字 の全体を中央へ揃える
		const float gap = kRankGap * uiScale;
		const float rowLeft = centerX - (labelWidth + gap + letterWidth) * 0.5f;
		rankLabelSprite_->SetSize({ labelWidth, labelHeight });
		rankLabelSprite_->SetPos({ rowLeft + labelWidth * 0.5f, rankY });
		rankSprite_->SetSize({ letterWidth, letterHeight });
		rankSprite_->SetPos({ rowLeft + labelWidth + gap + letterWidth * 0.5f, rankY });
		rankSprite_->SetColor(RankColor(gPendingRank));
	}
}

void ResultScene::Finalize() {
}

void ResultScene::Update(float deltaTime) {
	elapsed_ += deltaTime;

	// 操作案内はゆっくり明滅させる(押せる状態になったことを画面だけで伝える)
	if (elapsed_ >= inputLockDuration_) {
		const float alpha = 0.55f + 0.45f * std::sin(elapsed_ * 4.0f);
		Vector4 color = retrySprite_->GetColor();
		color.w = alpha;
		retrySprite_->SetColor(color);

		if (IsRetryPressed()) {
			// 新しいGamePlaySceneが作られるので、HP・敵・弾・進行度はすべて初期状態に戻る
			SceneManager::GetInstance()->ChangeScene("GAMEPLAY", std::make_unique<BlindTransition>());
		}
	} else {
		Vector4 color = retrySprite_->GetColor();
		color.w = 0.0f; // 入力猶予の間は案内を出さない
		retrySprite_->SetColor(color);
	}

	// 頂点と行列の書き込みはUpdateが行うため、Drawの前に毎フレーム呼ぶ
	resultSprite_->Update();
	retrySprite_->Update();
	scoreNumber_->Update();
	if (rankSprite_) {
		rankLabelSprite_->Update();
		rankSprite_->Update();
	}
}

void ResultScene::Draw() {
	// 背景はシーンRTのクリア色(黒)のまま。スプライトは深度OFF・αブレンドON
	SpriteCommon::GetInstance()->CommonDrawSetting();
	resultSprite_->Draw();
	retrySprite_->Draw();
	scoreNumber_->Draw(); // 数字画像が未配置なら何も描かない
	if (rankSprite_) { // CLEARのときだけ作ってある
		rankLabelSprite_->Draw();
		rankSprite_->Draw();
	}
}

bool ResultScene::IsRetryPressed() const {
	Input* input = Input::GetInstance();
	return input->IsTriggerKey(DIK_RETURN) || input->IsTriggerKey(DIK_SPACE) ||
		input->IsTriggerPad(XINPUT_GAMEPAD_A) || input->IsTriggerPad(XINPUT_GAMEPAD_START);
}

ResultScene::~ResultScene() = default;
