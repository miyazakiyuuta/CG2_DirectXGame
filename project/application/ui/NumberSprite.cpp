#include "ui/NumberSprite.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"

#include <algorithm>
#include <filesystem>

namespace {
	// 画像に並んでいる数字の個数(0〜9)
	constexpr int kDigitVariations = 10;
}

void NumberSprite::Initialize(const std::string& texturePath, int digitCount,
	float sourceTop, float sourceHeight) {
	digits_.clear();
	valid_ = false;

	// 素材が未配置でも起動できるようにする。
	// TextureManager::LoadTextureは存在しないファイルでassertに落ちるため、先に確認する
	if (!std::filesystem::exists(texturePath) || digitCount <= 0) {
		return;
	}

	TextureManager::GetInstance()->LoadTexture(texturePath);

	digits_.reserve(static_cast<size_t>(digitCount));
	for (int i = 0; i < digitCount; ++i) {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(SpriteCommon::GetInstance(), texturePath);
		// Initialize直後のサイズ=テクスチャの実寸。1文字幅は横幅÷10。
		// 縦は指定範囲だけを切り出す(数字が画像の一部にしか描かれていない場合に効く)
		if (i == 0) {
			const Vector2 textureSize = sprite->GetSize();
			const float height = (sourceHeight > 0.0f) ? sourceHeight : textureSize.y;
			sourceTop_ = sourceTop;
			sourceDigitSize_ = { textureSize.x / static_cast<float>(kDigitVariations), height };
		}
		digits_.push_back(std::move(sprite));
	}

	valid_ = sourceDigitSize_.x > 0.0f;
	if (valid_) {
		ApplyLayout();
		SetValue(0);
	}
}

void NumberSprite::ApplyLayout() {
	for (size_t i = 0; i < digits_.size(); ++i) {
		digits_[i]->SetSize(digitSize_);
		digits_[i]->SetPos({
			position_.x + static_cast<float>(i) * (digitSize_.x + gap_),
			position_.y });
	}
}

void NumberSprite::SetValue(int value) {
	if (!valid_) {
		return;
	}
	value = (std::max)(value, 0);

	// 下の桁から埋める。桁あふれは上位を捨てる(表示幅を固定したいため)
	for (size_t i = digits_.size(); i > 0; --i) {
		const int digit = value % 10;
		value /= 10;
		// 数字画像は0が左端なので、切り出しの左上Xは「文字幅 × 数字」
		digits_[i - 1]->SetTextureLeftTop({ sourceDigitSize_.x * static_cast<float>(digit), sourceTop_ });
		digits_[i - 1]->SetTextureSize(sourceDigitSize_);
	}
}

void NumberSprite::SetPosition(const Vector2& position) {
	position_ = position;
	if (valid_) {
		ApplyLayout();
	}
}

void NumberSprite::SetDigitSize(const Vector2& size, float gap) {
	digitSize_ = size;
	gap_ = gap;
	if (valid_) {
		ApplyLayout();
	}
}

void NumberSprite::SetColor(const Vector4& color) {
	for (std::unique_ptr<Sprite>& sprite : digits_) {
		sprite->SetColor(color);
	}
}

void NumberSprite::Update() {
	if (!valid_) {
		return;
	}
	for (std::unique_ptr<Sprite>& sprite : digits_) {
		sprite->Update();
	}
}

void NumberSprite::Draw() {
	if (!valid_) {
		return;
	}
	for (std::unique_ptr<Sprite>& sprite : digits_) {
		sprite->Draw();
	}
}

NumberSprite::NumberSprite() = default;

NumberSprite::~NumberSprite() = default;
