#include "ui/Reticle.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "base/WinApp.h"
#include "math/Matrix4x4.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

#include <string>

namespace {
	// 下地の白テクスチャ。white1x1ではなくwhite2x2を使う理由はHUDと同じ
	// (TextureManagerがGenerateMipMapsを通すため、1x1はミップが1段しか作れずassertに落ちる)
	const std::string kReticleTexture = "resources/white2x2.png";

	// レイアウトは1280x720基準で決め、実解像度の倍率を掛けて使う
	// (kClientWidthは構成で変わる: Debug=1920x1080 / Release=1280x720)
	constexpr float kUiBaseWidth = 1280.0f;
	constexpr float kBarLength = 22.0f;    // 十字1本の長さ[px]
	constexpr float kBarThickness = 4.0f;  // 十字1本の太さ[px]
	constexpr float kCenterGap = 12.0f;    // 中心から各本の内側の端まで[px]
	constexpr float kCenterDotSize = 5.0f; // 中心点の1辺[px]
	// 縁取りの張り出し[px]。エディタのSceneビューは1920のRTを縮小表示するため、
	// これが無いと明るい背景の上で線が消える
	constexpr float kOutlineExpand = 2.0f;

	// 通常時は自弾と同じ水色(弾と照準が同じものだと分かる)。
	// 敵に重なっている間は暖色にして「狙えている」ことを画面から読み取れるようにする
	const Vector4 kNormalColor = { 0.4f, 1.0f, 1.0f, 0.95f };
	const Vector4 kLockedColor = { 1.0f, 0.35f, 0.2f, 1.0f };
	// 縁取りの色。黒を敷くと空・雪・岩肌のどれに重なっても輪郭が残る
	const Vector4 kOutlineColor = { 0.0f, 0.0f, 0.0f, 0.65f };

	// パーツの並び。位置と大きさをindexで決めるので、意味を名前で固定しておく
	enum ReticlePart {
		kPartLeft,
		kPartRight,
		kPartTop,
		kPartBottom,
		kPartCenter,
		kPartCount,
	};

	// index番のパーツの大きさ[px](1280基準)。縁取り側はexpand分だけ上乗せする
	Vector2 PartSize(int index, float expand) {
		if (index == kPartLeft || index == kPartRight) {
			return { kBarLength + expand, kBarThickness + expand };
		}
		if (index == kPartTop || index == kPartBottom) {
			return { kBarThickness + expand, kBarLength + expand };
		}
		return { kCenterDotSize + expand, kCenterDotSize + expand };
	}

	// 5枚組を1列分作る。色は毎フレーム上書きするのでここでは設定しない
	std::vector<std::unique_ptr<Sprite>> CreateParts() {
		std::vector<std::unique_ptr<Sprite>> parts;
		parts.reserve(kPartCount);
		for (int i = 0; i < kPartCount; ++i) {
			auto sprite = std::make_unique<Sprite>();
			sprite->Initialize(SpriteCommon::GetInstance(), kReticleTexture);
			// 中心基準にすると、位置を「狙点のスクリーン座標＋オフセット」でそのまま書ける
			sprite->SetAnchorPoint({ 0.5f, 0.5f });
			parts.push_back(std::move(sprite));
		}
		return parts;
	}
}

void Reticle::Initialize() {
	TextureManager::GetInstance()->LoadTexture(kReticleTexture);

	outlines_ = CreateParts();
	fills_ = CreateParts();
}

void Reticle::LayoutParts(std::vector<std::unique_ptr<Sprite>>& parts, const Vector2& center,
	float uiScale, float expand) const {
	// 中心から各本の中央までの距離。内側の端をkCenterGapに揃えるため半分の長さを足す
	const float offset = (kCenterGap + kBarLength * 0.5f) * uiScale;

	parts[kPartLeft]->SetPos({ center.x - offset, center.y });
	parts[kPartRight]->SetPos({ center.x + offset, center.y });
	parts[kPartTop]->SetPos({ center.x, center.y - offset });
	parts[kPartBottom]->SetPos({ center.x, center.y + offset });
	parts[kPartCenter]->SetPos(center);

	for (int i = 0; i < kPartCount; ++i) {
		const Vector2 size = PartSize(i, expand);
		parts[i]->SetSize({ size.x * uiScale, size.y * uiScale });
	}
}

void Reticle::Update(const Vector3& aimPoint, const Camera& camera) {
	// clip = (aimPoint, 1) × viewProjection(行ベクトル規約)。
	// Matrix4x4::Transformを使わない理由: あちらはwで割った結果しか返さないため、
	// カメラ背面(w<0)の点でも画面内に見える座標を返してしまう。
	// 背面を弾くにはwの符号が必要なので、ここだけ手で展開する
	// (engine/effect/RadialBlur::SetCenterWorldと同じ書き方)
	const auto& m = camera.GetViewProjectionMatrix().m;
	const float clipX = aimPoint.x * m[0][0] + aimPoint.y * m[1][0] + aimPoint.z * m[2][0] + m[3][0];
	const float clipY = aimPoint.x * m[0][1] + aimPoint.y * m[1][1] + aimPoint.z * m[2][1] + m[3][1];
	const float clipW = aimPoint.x * m[0][3] + aimPoint.y * m[1][3] + aimPoint.z * m[2][3] + m[3][3];

	visible_ = clipW > 1.0e-4f;
	if (!visible_) {
		return; // 背面。Drawごと省くので位置の更新も不要
	}

	// NDC(-1〜1) → シーンRTのピクセル。Yだけ反転する(NDCは上が+1、スクリーンは下が+)
	const Vector2 center = {
		(clipX / clipW * 0.5f + 0.5f) * static_cast<float>(WinApp::kClientWidth),
		(-clipY / clipW * 0.5f + 0.5f) * static_cast<float>(WinApp::kClientHeight) };

	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kUiBaseWidth;
	LayoutParts(outlines_, center, uiScale, kOutlineExpand);
	LayoutParts(fills_, center, uiScale, 0.0f);

	const Vector4& fillColor = lockedOn_ ? kLockedColor : kNormalColor;
	for (std::unique_ptr<Sprite>& part : outlines_) {
		part->SetColor(kOutlineColor);
		part->Update();
	}
	for (std::unique_ptr<Sprite>& part : fills_) {
		part->SetColor(fillColor);
		part->Update();
	}
}

void Reticle::Draw() {
	if (!visible_) {
		return;
	}
	// 縁取りを先に全部描いてから本体を重ねる(1パーツずつ交互に描くと、
	// 隣のパーツの縁取りが本体の上に乗って線が途切れて見える)
	for (const std::unique_ptr<Sprite>& part : outlines_) {
		part->Draw();
	}
	for (const std::unique_ptr<Sprite>& part : fills_) {
		part->Draw();
	}
}

Reticle::Reticle() = default;

Reticle::~Reticle() = default;
