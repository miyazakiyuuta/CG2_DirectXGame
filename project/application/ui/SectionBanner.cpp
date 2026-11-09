#include "ui/SectionBanner.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "base/WinApp.h"
#include "math/Easing.h"
#include "math/Vector2.h"
#include "math/Vector4.h"
#include "ui/NumberSprite.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
	// 見出しと区切りの画像(仮。Pythonで生成したもの。clear.png と同じ「白文字＋黒縁」の見た目に合わせてある)
	const std::string kPerfectTexture = "resources/ui/section_perfect.png";
	const std::string kClearTexture = "resources/ui/section_clear.png";
	// 「/」。numbers.png の1桁と同じく高さ724の中の y=228〜484 に描いてあるので、同じ範囲を切り出す
	const std::string kSlashTexture = "resources/ui/slash.png";
	const std::string kNumberTexture = "resources/ui/numbers.png";

	// レイアウトは1280x720基準で決め、実解像度の倍率を掛けて使う(HUDと同じ)
	constexpr float kUiBaseWidth = 1280.0f;
	// 画面の上の方(進行ゲージの下)。中央の照準と敵に重ならない高さにする
	constexpr float kLabelCenterY = 120.0f;  // 見出しの中心[px]
	constexpr float kLabelHeight = 44.0f;    // 見出しの高さ[px](幅は画像の縦横比から決める)
	constexpr float kNumberTopGap = 10.0f;   // 見出しの下端から数字の上端まで[px]
	// 1桁の表示サイズ[px]。HUDのスコア(30x36)と同じ比率0.85
	constexpr float kDigitWidth = 26.0f;
	constexpr float kDigitHeight = 31.0f;
	constexpr float kNumberGap = 6.0f;       // 「n」「/」「m」の間隔[px]

	// 表示の時間[秒]。出だしだけ大きめから縮めて「弾む」ように出し、最後に薄くして消す
	constexpr float kDuration = 2.0f;
	constexpr float kFadeInDuration = 0.12f;
	constexpr float kFadeOutDuration = 0.45f;
	constexpr float kPopDuration = 0.25f;
	constexpr float kPopStartScale = 1.35f;

	// PERFECTは進行ゲージのPERFECTの帯と同じ金色(画像は白文字なので、色を掛けると文字だけが金色になる)
	const Vector4 kPerfectColor = { 1.0f, 0.85f, 0.2f, 1.0f };
	const Vector4 kClearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 1桁用と2桁用のどちらを使うか(0〜9は1桁)
	NumberSprite& PickDigits(int value, NumberSprite& oneDigit, NumberSprite& twoDigits) {
		return value < 10 ? oneDigit : twoDigits;
	}

	int DigitCountOf(int value) {
		return value < 10 ? 1 : 2;
	}
}

void SectionBanner::Initialize() {
	TextureManager::GetInstance()->LoadTexture(kPerfectTexture);
	TextureManager::GetInstance()->LoadTexture(kClearTexture);
	TextureManager::GetInstance()->LoadTexture(kSlashTexture);

	auto createLabel = [](const std::string& texture) {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(SpriteCommon::GetInstance(), texture);
		sprite->SetAnchorPoint({ 0.5f, 0.5f }); // 中心基準にすると、弾む動き(拡大縮小)の中心がぶれない
		return sprite;
	};
	perfectLabel_ = createLabel(kPerfectTexture);
	clearLabel_ = createLabel(kClearTexture);
	// Initialize直後のサイズ=画像の実寸(Sprite::AdjustTextureSize)なので、ここで縦横比を控える
	auto aspectOf = [](const Sprite& sprite) {
		const Vector2& size = sprite.GetSize();
		return size.y > 0.0f ? size.x / size.y : 1.0f;
	};
	perfectAspect_ = aspectOf(*perfectLabel_);
	clearAspect_ = aspectOf(*clearLabel_);

	auto createNumber = [](int digitCount) {
		auto number = std::make_unique<NumberSprite>();
		number->Initialize(kNumberTexture, digitCount);
		return number;
	};
	defeatedOneDigit_ = createNumber(1);
	defeatedTwoDigits_ = createNumber(2);
	totalOneDigit_ = createNumber(1);
	totalTwoDigits_ = createNumber(2);

	slash_ = std::make_unique<Sprite>();
	slash_->Initialize(SpriteCommon::GetInstance(), kSlashTexture);
	const float slashTextureWidth = slash_->GetSize().x; // Initialize直後=画像の実寸
	slash_->SetTextureLeftTop({ 0.0f, NumberSprite::kDefaultSourceTop });
	slash_->SetTextureSize({ slashTextureWidth, NumberSprite::kDefaultSourceHeight });
	slashAspect_ = slashTextureWidth / NumberSprite::kDefaultSourceHeight;
}

void SectionBanner::Show(const StageProgress::SectionResult& result) {
	perfect_ = result.perfect;
	// 2桁までしか出せないので丸める(1区間に100体は出さない前提)
	defeated_ = std::clamp(result.defeated, 0, 99);
	total_ = std::clamp(result.total, 0, 99);
	timer_ = 0.0f;
	active_ = true;
}

void SectionBanner::Update(float deltaTime) {
	if (!active_) {
		return;
	}
	timer_ += deltaTime;
	if (timer_ >= kDuration) {
		active_ = false;
		return;
	}

	// 濃さ: 出だしで素早く現れ、最後のkFadeOutDurationで消えていく。HUD全体の濃さも掛ける
	const float fadeIn = std::clamp(timer_ / kFadeInDuration, 0.0f, 1.0f);
	const float fadeOut = std::clamp((kDuration - timer_) / kFadeOutDuration, 0.0f, 1.0f);
	const float alpha = fadeIn * fadeOut * alpha_;
	const Vector4 baseColor = perfect_ ? kPerfectColor : kClearColor;
	const Vector4 color = { baseColor.x, baseColor.y, baseColor.z, baseColor.w * alpha };

	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kUiBaseWidth;
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float labelCenterY = kLabelCenterY * uiScale;

	// 見出し。大きめ(kPopStartScale倍)から等倍へ、減速しながら縮める
	const float pop = std::lerp(kPopStartScale, 1.0f, Easing::OutCubic(timer_ / kPopDuration));
	Sprite& label = perfect_ ? *perfectLabel_ : *clearLabel_;
	const float labelHeight = kLabelHeight * uiScale;
	const float aspect = perfect_ ? perfectAspect_ : clearAspect_;
	label.SetPos({ centerX, labelCenterY });
	label.SetSize({ labelHeight * aspect * pop, labelHeight * pop });
	label.SetColor(color);
	label.Update();

	// 「n / m」を見出しの下に中央揃えで並べる。幅は桁数で変わるので、先に全体の幅を出してから左端を決める
	const float digitWidth = kDigitWidth * uiScale;
	const float digitHeight = kDigitHeight * uiScale;
	const float gap = kNumberGap * uiScale;
	// 「/」の幅は、切り出す帯(数字と同じ高さの範囲)の縦横比から決める
	const float slashDisplayWidth = digitHeight * slashAspect_;
	const float defeatedWidth = digitWidth * static_cast<float>(DigitCountOf(defeated_));
	const float totalWidth = digitWidth * static_cast<float>(DigitCountOf(total_));
	const float rowWidth = defeatedWidth + gap + slashDisplayWidth + gap + totalWidth;
	const float rowTop = labelCenterY + labelHeight * 0.5f + kNumberTopGap * uiScale;
	float x = centerX - rowWidth * 0.5f;

	NumberSprite& defeated = PickDigits(defeated_, *defeatedOneDigit_, *defeatedTwoDigits_);
	defeated.SetDigitSize({ digitWidth, digitHeight });
	defeated.SetPosition({ x, rowTop });
	defeated.SetValue(defeated_);
	defeated.SetColor(color);
	defeated.Update();
	x += defeatedWidth + gap;

	slash_->SetPos({ x, rowTop });
	slash_->SetSize({ slashDisplayWidth, digitHeight });
	slash_->SetColor(color);
	slash_->Update();
	x += slashDisplayWidth + gap;

	NumberSprite& total = PickDigits(total_, *totalOneDigit_, *totalTwoDigits_);
	total.SetDigitSize({ digitWidth, digitHeight });
	total.SetPosition({ x, rowTop });
	total.SetValue(total_);
	total.SetColor(color);
	total.Update();
}

void SectionBanner::Draw() {
	if (!active_) {
		return;
	}
	(perfect_ ? perfectLabel_ : clearLabel_)->Draw();
	PickDigits(defeated_, *defeatedOneDigit_, *defeatedTwoDigits_).Draw();
	slash_->Draw();
	PickDigits(total_, *totalOneDigit_, *totalTwoDigits_).Draw();
}

SectionBanner::SectionBanner() = default;
SectionBanner::~SectionBanner() = default;
