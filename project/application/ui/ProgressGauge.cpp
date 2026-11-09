#include "ui/ProgressGauge.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "base/WinApp.h"
#include "math/Vector2.h"
#include "math/Vector4.h"

#include <algorithm>
#include <string>

namespace {
	// 下地の白テクスチャ(white1x1ではなくwhite2x2を使う理由はHUD・Reticleと同じ)
	const std::string kGaugeTexture = "resources/white2x2.png";

	// レイアウトは1280x720基準で決め、実解像度の倍率を掛けて使う
	constexpr float kUiBaseWidth = 1280.0f;
	// 画面上部の中央。左上のHP・右上のスコアの間に収まる幅と、それらと揃う高さにする
	constexpr float kGaugeWidth = 400.0f;  // 帯の長さ[px]
	constexpr float kGaugeHeight = 6.0f;   // 帯の太さ[px]
	constexpr float kGaugeCenterY = 36.0f; // 帯の中心の高さ[px](HPのマスの中心と揃える)
	// 縁取りの張り出し[px]。Reticleと同じく、明るい背景の上でも輪郭が消えないように黒を一回り大きく敷く
	constexpr float kOutlineExpand = 2.0f;
	constexpr float kTickWidth = 2.0f;   // 区切りの目盛りの幅[px]
	constexpr float kTickHeight = 14.0f; // 区切りの目盛りの高さ[px](帯より上下に張り出して、区切りだと分かるように)
	constexpr float kMarkerSize = 10.0f; // 自機の今の位置の印の1辺[px]
	constexpr float kGoalSize = 12.0f;   // 終点(女王)の印の1辺[px]。自機の印より少し大きく、目標だと分かるように

	const Vector4 kOutlineColor = { 0.0f, 0.0f, 0.0f, 0.65f };
	const Vector4 kTrackColor = { 0.25f, 0.25f, 0.25f, 0.8f }; // まだ進んでいない所
	const Vector4 kFillColor = { 0.9f, 0.9f, 0.9f, 0.9f };     // 進んだ所
	// 取り逃しなしで終えた区間。区間の目標を達成したことを、ゲージの上にずっと残す
	const Vector4 kPerfectColor = { 1.0f, 0.85f, 0.2f, 1.0f };
	const Vector4 kTickColor = { 1.0f, 1.0f, 1.0f, 0.95f };
	// 自機の印は自弾・照準と同じ水色(「これが自分」と照準と同じ色で読める)
	const Vector4 kMarkerColor = { 0.4f, 1.0f, 1.0f, 1.0f };
	// 終点の印。女王がいれば敵の色(赤)、いなければ帯と同じ白
	const Vector4 kGoalColor = { 1.0f, 0.3f, 0.2f, 1.0f };
	const Vector4 kEndColor = { 0.9f, 0.9f, 0.9f, 0.9f };

	std::unique_ptr<Sprite> CreatePart(const Vector2& anchor) {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(SpriteCommon::GetInstance(), kGaugeTexture);
		sprite->SetAnchorPoint(anchor);
		return sprite;
	}

	// 位置・大きさ・色をまとめて入れ、頂点を更新する。色には全体の透明度を掛ける
	void Place(Sprite& sprite, const Vector2& pos, const Vector2& size, const Vector4& color, float alpha) {
		sprite.SetPos(pos);
		sprite.SetSize(size);
		sprite.SetColor({ color.x, color.y, color.z, color.w * alpha });
		sprite.Update();
	}

	// 帯は左端・縦中央を基準にする(長さを変えるだけで右へ伸びる)。印と目盛りは中心を基準にする
	const Vector2 kAnchorLeft = { 0.0f, 0.5f };
	const Vector2 kAnchorCenter = { 0.5f, 0.5f };
}

void ProgressGauge::Initialize() {
	TextureManager::GetInstance()->LoadTexture(kGaugeTexture);

	outline_ = CreatePart(kAnchorLeft);
	track_ = CreatePart(kAnchorLeft);
	fill_ = CreatePart(kAnchorLeft);
	goalOutline_ = CreatePart(kAnchorCenter);
	goal_ = CreatePart(kAnchorCenter);
	markerOutline_ = CreatePart(kAnchorCenter);
	marker_ = CreatePart(kAnchorCenter);
}

void ProgressGauge::EnsureSectionParts(size_t count) {
	while (perfectBands_.size() < count) {
		perfectBands_.push_back(CreatePart(kAnchorLeft));
		tickOutlines_.push_back(CreatePart(kAnchorCenter));
		ticks_.push_back(CreatePart(kAnchorCenter));
	}
}

void ProgressGauge::Update(const std::vector<StageProgress::SectionResult>& sections, float railDistance,
	float railLength, bool hasGoal) {
	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kUiBaseWidth;
	const float width = kGaugeWidth * uiScale;
	const float height = kGaugeHeight * uiScale;
	const float left = (static_cast<float>(WinApp::kClientWidth) - width) * 0.5f;
	const float centerY = kGaugeCenterY * uiScale;
	const float outline = kOutlineExpand * uiScale;

	// 進行度[m] → 帯の上の横位置[px]。全長が0(レールなし)なら左端に置く
	auto toX = [&](float distance) {
		const float ratio = railLength > 0.0f ? std::clamp(distance / railLength, 0.0f, 1.0f) : 0.0f;
		return left + width * ratio;
	};

	Place(*outline_, { left - outline, centerY }, { width + outline * 2.0f, height + outline * 2.0f },
		kOutlineColor, alpha_);
	Place(*track_, { left, centerY }, { width, height }, kTrackColor, alpha_);
	Place(*fill_, { left, centerY }, { toX(railDistance) - left, height }, kFillColor, alpha_);

	// 区間ごと: PERFECTで確定した区間はその区間の幅を金色で塗る(進んだ所より先まで塗ることもある:
	// 区間の敵を先に倒し切れば、その場で区間の目標を達成したことが見える)。
	// 区切りの目盛りは各区間の始まりに置く(先頭はゲージの左端そのものなので置かない)
	sectionCount_ = sections.size();
	EnsureSectionParts(sectionCount_);
	for (size_t i = 0; i < sectionCount_; ++i) {
		const StageProgress::SectionResult& section = sections[i];
		const float startX = toX(section.startDistance);
		const float endX = toX(section.endDistance);
		Place(*perfectBands_[i], { startX, centerY }, { endX - startX, height }, kPerfectColor,
			section.perfect ? alpha_ : 0.0f);

		const float tickAlpha = i > 0 ? alpha_ : 0.0f;
		Place(*tickOutlines_[i], { startX, centerY },
			{ (kTickWidth + kOutlineExpand * 2.0f) * uiScale, (kTickHeight + kOutlineExpand * 2.0f) * uiScale },
			kOutlineColor, tickAlpha);
		Place(*ticks_[i], { startX, centerY }, { kTickWidth * uiScale, kTickHeight * uiScale }, kTickColor, tickAlpha);
	}

	// 終点の印(女王がいれば赤)と、自機の今の位置の印。自機の印を最後に描いて、終点に着いたときも上に重ねる
	const float rightX = left + width;
	const float goalSize = kGoalSize * uiScale;
	Place(*goalOutline_, { rightX, centerY }, { goalSize + outline * 2.0f, goalSize + outline * 2.0f },
		kOutlineColor, alpha_);
	Place(*goal_, { rightX, centerY }, { goalSize, goalSize }, hasGoal ? kGoalColor : kEndColor, alpha_);

	const float markerX = toX(railDistance);
	const float markerSize = kMarkerSize * uiScale;
	Place(*markerOutline_, { markerX, centerY }, { markerSize + outline * 2.0f, markerSize + outline * 2.0f },
		kOutlineColor, alpha_);
	Place(*marker_, { markerX, centerY }, { markerSize, markerSize }, kMarkerColor, alpha_);
}

void ProgressGauge::Draw() {
	// 奥から順に: 土台 → 進んだ所 → PERFECTの帯 → 区切り → 終点 → 自機
	outline_->Draw();
	track_->Draw();
	fill_->Draw();
	for (size_t i = 0; i < sectionCount_; ++i) {
		perfectBands_[i]->Draw();
	}
	for (size_t i = 0; i < sectionCount_; ++i) {
		tickOutlines_[i]->Draw();
		ticks_[i]->Draw();
	}
	goalOutline_->Draw();
	goal_->Draw();
	markerOutline_->Draw();
	marker_->Draw();
}

ProgressGauge::ProgressGauge() = default;
ProgressGauge::~ProgressGauge() = default;
