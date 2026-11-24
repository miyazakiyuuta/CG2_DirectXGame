#include "scene/ClearResultScene.h"

#include "base/WinApp.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"

#include <algorithm>

namespace {
	// GamePlaySceneから渡される表示用のランク。
	// ClearResultSceneは遷移のたびに作り直されるので、実体ではなくここで預かる(スコアと同じ)
	StageProgress::Rank gPendingRank = StageProgress::Rank::C;

	// ランク(仮の画像。Pythonで生成したもの。clear.pngと同じ「白文字＋黒縁」)。
	// ranks.png は S/A/B/C を左から等幅に並べてあり、Rankの並び(S=0〜C=3)の位置を切り出す
	const std::string kRankTexture = "resources/ui/ranks.png";
	const std::string kRankLabelTexture = "resources/ui/rank_label.png";
	constexpr int kRankCount = 4;
	constexpr float kRankY = 0.45f;             // ランクの段の中心(画面の高さに対する割合)
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
}

void ClearResultScene::SetRank(StageProgress::Rank rank) {
	gPendingRank = rank;
}

ClearResultScene::ClearResultScene() = default;

ClearResultScene::~ClearResultScene() = default;

std::string ClearResultScene::GetResultTexturePath() const {
	return "resources/ui/clear.png";
}

Vector4 ClearResultScene::GetResultColor() const {
	return { 1.0f, 1.0f, 1.0f, 1.0f }; // 白(ゲームオーバーの赤みと一目で区別する)
}

std::string ClearResultScene::GetResultSoundPath() const {
	return "resources/sounds/clear.mp3";
}

ResultScene::Layout ClearResultScene::GetLayout() const {
	// ランクの段(kRankY)が文言とスコアの間に入るぶん、全体を上へ詰める
	Layout layout;
	layout.resultY = 0.28f;
	layout.scoreTop = 0.57f;
	layout.menuTopY = 0.73f;
	return layout;
}

void ClearResultScene::Initialize() {
	// 共通部分(文言・選択肢・結果音・スコア)を先に作り、その後にランクの段を足す
	ResultScene::Initialize();

	const float uiScale = GetUiScale();
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float rankY = static_cast<float>(WinApp::kClientHeight) * kRankY;

	// ランク(駆除報告)。「RANK」と文字を横に並べて中央に置く
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

void ClearResultScene::Update(float deltaTime) {
	ResultScene::Update(deltaTime);
	// 頂点と行列の書き込みはUpdateが行うため、Drawの前に毎フレーム呼ぶ
	rankLabelSprite_->Update();
	rankSprite_->Update();
}

void ClearResultScene::Draw() {
	// 基底が描画の共通設定(CommonDrawSetting)を済ませた後なので、続けてスプライトを描ける
	ResultScene::Draw();
	rankLabelSprite_->Draw();
	rankSprite_->Draw();
}
