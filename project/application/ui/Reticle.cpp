#include "ui/Reticle.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "base/WinApp.h"
#include "math/Easing.h"
#include "math/Matrix4x4.h"
#include "math/Vector4.h"

#include <algorithm>
#include <cmath>
#include <numbers>
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
	const Vector4 kAimedColor = { 1.0f, 0.35f, 0.2f, 1.0f };
	// 縁取りの色。黒を敷くと空・雪・岩肌のどれに重なっても輪郭が残る
	const Vector4 kOutlineColor = { 0.0f, 0.0f, 0.0f, 0.65f };

	// チャージの表示。溜まるほど十字の隙間を詰め、チャージ弾と同じ黄緑へ寄せる
	// (「離すとこの色の弾が出る」ことを照準から読み取れるようにする)
	constexpr float kChargedGap = 4.0f; // 満タン時の、中心から各本の内側の端まで[px]
	const Vector4 kChargeColor = { 0.6f, 1.0f, 0.3f, 1.0f };
	// 満タン時の脈打ち。周波数[Hz]と、大きさの振れ幅(1.0〜1.0+この割合)
	constexpr float kPulseFrequency = 5.0f;
	constexpr float kPulseAmount = 0.2f;

	// 吸い付きの追従の速さ[1/秒]。大きいほど、引き寄せ先へのずれに素早く追いつく
	// (18なら約0.05秒で残りのずれが1/e(約37%)になる)
	constexpr float kAssistFollowSpeed = 18.0f;

	// ロック枠。敵を囲む四角の四隅にカギ型を置く。色は「狙えている」と同じ暖色の赤寄り
	const Vector4 kLockColor = { 1.0f, 0.25f, 0.15f, 1.0f };
	constexpr float kLockBarThickness = 3.0f;   // カギの太さ[px]
	constexpr float kLockBarMinLength = 10.0f;  // カギの1辺の長さの下限・上限[px]
	constexpr float kLockBarMaxLength = 36.0f;
	constexpr float kLockMinHalfSize = 20.0f;   // 遠くの小さな敵でも枠が潰れない最小の大きさ[px]
	// ロックした瞬間、枠を大きめから敵の大きさまで縮めて「吸い付く」動きにする
	constexpr float kLockSnapDuration = 0.15f;  // [秒]
	constexpr float kLockSnapStartScale = 2.2f; // 動き始めの大きさ(敵の大きさに対する倍率)

	// 十字のパーツの並び。位置と大きさをindexで決めるので、意味を名前で固定しておく
	enum ReticlePart {
		kPartLeft,
		kPartRight,
		kPartTop,
		kPartBottom,
		kPartCenter,
		kPartCount,
	};
	// ロック枠のパーツ数(四隅×横棒・縦棒)
	constexpr int kLockPartCount = 8;

	// index番のパーツの大きさ[px](1280基準)。脈打つ倍率を掛け、縁取り側はexpand分だけ上乗せする
	Vector2 PartSize(int index, float sizeScale, float expand) {
		const float length = kBarLength * sizeScale;
		const float thickness = kBarThickness * sizeScale;
		if (index == kPartLeft || index == kPartRight) {
			return { length + expand, thickness + expand };
		}
		if (index == kPartTop || index == kPartBottom) {
			return { thickness + expand, length + expand };
		}
		const float dot = kCenterDotSize * sizeScale;
		return { dot + expand, dot + expand };
	}

	// count枚のパーツを1列分作る。色は毎フレーム上書きするのでここでは設定しない
	std::vector<std::unique_ptr<Sprite>> CreateParts(int count) {
		std::vector<std::unique_ptr<Sprite>> parts;
		parts.reserve(count);
		for (int i = 0; i < count; ++i) {
			auto sprite = std::make_unique<Sprite>();
			sprite->Initialize(SpriteCommon::GetInstance(), kReticleTexture);
			// 中心基準にすると、位置を「投影したスクリーン座標＋オフセット」でそのまま書ける
			sprite->SetAnchorPoint({ 0.5f, 0.5f });
			parts.push_back(std::move(sprite));
		}
		return parts;
	}

	// 1列分のパーツへ色を入れて頂点を更新する
	void ApplyColor(std::vector<std::unique_ptr<Sprite>>& parts, const Vector4& color) {
		for (std::unique_ptr<Sprite>& part : parts) {
			part->SetColor(color);
			part->Update();
		}
	}

	Vector4 LerpColor(const Vector4& from, const Vector4& to, float t) {
		return {
			std::lerp(from.x, to.x, t),
			std::lerp(from.y, to.y, t),
			std::lerp(from.z, to.z, t),
			std::lerp(from.w, to.w, t) };
	}

	/// <summary>
	/// ワールド座標をシーンRTのピクセル座標へ投影する。カメラの背面ならfalse(画面に出してはいけない点)。
	/// clip = (world, 1) × viewProjection(行ベクトル規約)。
	/// Matrix4x4::Transformを使わない理由: あちらはwで割った結果しか返さないため、
	/// カメラ背面(w<0)の点でも画面内に見える座標を返してしまう。
	/// 背面を弾くにはwの符号が必要なので、ここだけ手で展開する
	/// (engine/effect/RadialBlur::SetCenterWorldと同じ書き方)
	/// </summary>
	bool ProjectToScreen(const Vector3& world, const Camera& camera, Vector2& outScreen) {
		const auto& m = camera.GetViewProjectionMatrix().m;
		const float clipX = world.x * m[0][0] + world.y * m[1][0] + world.z * m[2][0] + m[3][0];
		const float clipY = world.x * m[0][1] + world.y * m[1][1] + world.z * m[2][1] + m[3][1];
		const float clipW = world.x * m[0][3] + world.y * m[1][3] + world.z * m[2][3] + m[3][3];
		if (clipW <= 1.0e-4f) {
			return false;
		}
		// NDC(-1〜1) → シーンRTのピクセル。Yだけ反転する(NDCは上が+1、スクリーンは下が+)
		outScreen = {
			(clipX / clipW * 0.5f + 0.5f) * static_cast<float>(WinApp::kClientWidth),
			(-clipY / clipW * 0.5f + 0.5f) * static_cast<float>(WinApp::kClientHeight) };
		return true;
	}
}

void Reticle::Initialize() {
	TextureManager::GetInstance()->LoadTexture(kReticleTexture);

	outlines_ = CreateParts(kPartCount);
	fills_ = CreateParts(kPartCount);
	lockOutlines_ = CreateParts(kLockPartCount);
	lockFills_ = CreateParts(kLockPartCount);
}

void Reticle::LayoutParts(std::vector<std::unique_ptr<Sprite>>& parts, const Vector2& center,
	float uiScale, float gap, float sizeScale, float expand) const {
	// 中心から各本の中央までの距離。内側の端をgapに揃えるため半分の長さを足す
	const float offset = (gap + kBarLength * sizeScale * 0.5f) * uiScale;

	parts[kPartLeft]->SetPos({ center.x - offset, center.y });
	parts[kPartRight]->SetPos({ center.x + offset, center.y });
	parts[kPartTop]->SetPos({ center.x, center.y - offset });
	parts[kPartBottom]->SetPos({ center.x, center.y + offset });
	parts[kPartCenter]->SetPos(center);

	for (int i = 0; i < kPartCount; ++i) {
		const Vector2 size = PartSize(i, sizeScale, expand);
		parts[i]->SetSize({ size.x * uiScale, size.y * uiScale });
	}
}

void Reticle::LayoutLockParts(std::vector<std::unique_ptr<Sprite>>& parts, const Vector2& center,
	float halfSize, float uiScale, float expand) const {
	// カギの1辺は枠の大きさに比例させ(大きな敵ほど大きなカギ)、極端にならないよう上下限で丸める
	const float length = std::clamp(halfSize * 0.5f, kLockBarMinLength * uiScale, kLockBarMaxLength * uiScale);
	const float thickness = kLockBarThickness * uiScale;
	const float outline = expand * uiScale;

	// 四隅の向き(右下・左下・右上・左上)。各角に横棒と縦棒を1本ずつ置き、角で重ねてカギ型にする
	const float kCornerSigns[4][2] = { { 1.0f, 1.0f }, { -1.0f, 1.0f }, { 1.0f, -1.0f }, { -1.0f, -1.0f } };
	for (int corner = 0; corner < 4; ++corner) {
		const float signX = kCornerSigns[corner][0];
		const float signY = kCornerSigns[corner][1];
		// 横棒: 角から内側へlengthだけ伸びる(中心は角から半分の長さだけ内側)
		Sprite* horizontal = parts[corner * 2].get();
		horizontal->SetPos({ center.x + signX * (halfSize - length * 0.5f), center.y + signY * halfSize });
		horizontal->SetSize({ length + outline, thickness + outline });
		// 縦棒: 同じ角から縦に伸びる
		Sprite* vertical = parts[corner * 2 + 1].get();
		vertical->SetPos({ center.x + signX * halfSize, center.y + signY * (halfSize - length * 0.5f) });
		vertical->SetSize({ thickness + outline, length + outline });
	}
}

void Reticle::SetLockTarget(uint32_t targetId, const Vector3& center, float radius) {
	// 相手が変わった(新しくロックした・ロックが外れた・別の敵へ移った)ら、吸い付く動きを最初から
	if (targetId != lockTargetId_) {
		lockTime_ = 0.0f;
	}
	lockTargetId_ = targetId;
	lockCenter_ = center;
	lockRadius_ = radius;
}

void Reticle::SetAssistTarget(const Vector3& target, float amount) {
	assistTarget_ = target;
	assistAmount_ = amount;
}

void Reticle::Update(const Vector3& aimPoint, const Camera& camera, float deltaTime) {
	pulseTime_ += deltaTime;
	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kUiBaseWidth;

	// --- 十字 ---
	Vector2 center{};
	visible_ = ProjectToScreen(aimPoint, camera, center);
	if (visible_) {
		// 吸い付き。目標のずれ = (引き寄せ先の画面位置 - 本来の位置) × 強さ。
		// 今のずれをそこへ「1 - e^(-速さ×dt)」の割合ずつ近づける(指数的な追従)。
		// この割合は時間だけで決まるので、フレームレートが変わっても追いつく速さが変わらない
		Vector2 targetOffset{};
		Vector2 assistScreen{};
		if (assistAmount_ > 0.0f && ProjectToScreen(assistTarget_, camera, assistScreen)) {
			const float amount = std::clamp(assistAmount_, 0.0f, 1.0f);
			targetOffset = { (assistScreen.x - center.x) * amount, (assistScreen.y - center.y) * amount };
		}
		const float follow = 1.0f - std::exp(-kAssistFollowSpeed * deltaTime);
		assistOffset_.x += (targetOffset.x - assistOffset_.x) * follow;
		assistOffset_.y += (targetOffset.y - assistOffset_.y) * follow;
		center.x += assistOffset_.x;
		center.y += assistOffset_.y;

		// チャージ: 溜まるほど隙間を詰める。満タンになったら大きさを一定周期で脈打たせ、
		// 「離せばチャージ弾が出る」ことを知らせる
		const float charge = std::clamp(chargeRatio_, 0.0f, 1.0f);
		const float gap = std::lerp(kCenterGap, kChargedGap, charge);
		float sizeScale = 1.0f;
		if (charge >= 1.0f) {
			const float wave = std::sin(2.0f * std::numbers::pi_v<float> * kPulseFrequency * pulseTime_);
			sizeScale += kPulseAmount * (0.5f + 0.5f * wave);
		}
		LayoutParts(outlines_, center, uiScale, gap, sizeScale, kOutlineExpand);
		LayoutParts(fills_, center, uiScale, gap, sizeScale, 0.0f);

		// 色は状態で決め、濃さだけ外からの倍率(HUDのフェードイン)を掛ける。
		// チャージ中は、今の色(通常/敵に重なっている)からチャージ弾の色へ寄せていく
		Vector4 fillColor = LerpColor(aimingAtEnemy_ ? kAimedColor : kNormalColor, kChargeColor, charge);
		fillColor.w *= alpha_;
		Vector4 outlineColor = kOutlineColor;
		outlineColor.w *= alpha_;
		ApplyColor(outlines_, outlineColor);
		ApplyColor(fills_, fillColor);
	}

	// --- ロック枠 ---
	Vector2 lockCenter{};
	lockVisible_ = lockTargetId_ != 0 && ProjectToScreen(lockCenter_, camera, lockCenter);
	if (lockVisible_) {
		lockTime_ += deltaTime;

		// 画面上の敵の大きさ = 「敵の中心」と「中心から半径ぶん上の点」を両方投影したときの距離。
		// 遠い敵ほど小さく、近い敵ほど大きく囲める。上の点が背面に回るほど近い場合は最小の大きさにする
		float halfSize = kLockMinHalfSize * uiScale;
		Vector2 edge{};
		if (ProjectToScreen(lockCenter_ + Vector3{ 0.0f, lockRadius_, 0.0f }, camera, edge)) {
			const float dx = edge.x - lockCenter.x;
			const float dy = edge.y - lockCenter.y;
			halfSize = (std::max)(std::sqrt(dx * dx + dy * dy), halfSize);
		}
		// 吸い付く動き: ロックした瞬間は大きく、kLockSnapDuration秒かけて敵の大きさまで縮める(最後は減速)
		halfSize *= std::lerp(kLockSnapStartScale, 1.0f, Easing::OutCubic(lockTime_ / kLockSnapDuration));

		LayoutLockParts(lockOutlines_, lockCenter, halfSize, uiScale, kOutlineExpand);
		LayoutLockParts(lockFills_, lockCenter, halfSize, uiScale, 0.0f);

		Vector4 lockColor = kLockColor;
		lockColor.w *= alpha_;
		Vector4 outlineColor = kOutlineColor;
		outlineColor.w *= alpha_;
		ApplyColor(lockOutlines_, outlineColor);
		ApplyColor(lockFills_, lockColor);
	}
}

void Reticle::Draw() {
	// 縁取りを先に全部描いてから本体を重ねる(1パーツずつ交互に描くと、
	// 隣のパーツの縁取りが本体の上に乗って線が途切れて見える)。
	// ロック枠を先に描き、照準の十字が枠より手前に来るようにする
	if (lockVisible_) {
		for (const std::unique_ptr<Sprite>& part : lockOutlines_) {
			part->Draw();
		}
		for (const std::unique_ptr<Sprite>& part : lockFills_) {
			part->Draw();
		}
	}
	if (!visible_) {
		return;
	}
	for (const std::unique_ptr<Sprite>& part : outlines_) {
		part->Draw();
	}
	for (const std::unique_ptr<Sprite>& part : fills_) {
		part->Draw();
	}
}

Reticle::Reticle() = default;

Reticle::~Reticle() = default;
