#define NOMINMAX
#include "UI/SpriteNumberText.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"

#include <algorithm>

void SpriteNumberText::Initialize(
    SpriteCommon* spriteCommon,
    const std::string& textureFilePath,
    int maxDigits
)
{
    spriteCommon_ = spriteCommon;
    textureFilePath_ = textureFilePath;

    digitSprites_.clear();
    digitSprites_.reserve(maxDigits);

    for (int i = 0; i < maxDigits; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_, textureFilePath_);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        sprite->SetSize(digitSize_);
        sprite->SetTextureSize(textureCellSize_);
        sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        digitSprites_.push_back(std::move(sprite));
    }
}

Vector2 SpriteNumberText::GetTextureLeftTopFromDigit(int digit) const
{
    // 画像の並びが 1 2 3 4 5 6 7 8 9 0 なので、
    // 0 は最後のセル、1〜9 は digit-1 番目のセル
    int index = 9;
    if (digit >= 1 && digit <= 9) {
        index = digit - 1;
    }

    return {
        textureCellSize_.x * static_cast<float>(index),
        0.0f
    };
}

void SpriteNumberText::SetNumber(int value, int minDigits)
{
    value = std::max(0, value);

    currentText_ = std::to_string(value);

    while (static_cast<int>(currentText_.size()) < minDigits) {
        currentText_ = "0" + currentText_;
    }

    if (currentText_.size() > digitSprites_.size()) {
        currentText_ = currentText_.substr(currentText_.size() - digitSprites_.size());
    }

    for (size_t i = 0; i < digitSprites_.size(); ++i) {
        Sprite* sprite = digitSprites_[i].get();

        if (i >= currentText_.size()) {
            sprite->SetColor({ color_.x, color_.y, color_.z, 0.0f });
            continue;
        }

        int digit = currentText_[i] - '0';

        sprite->SetColor(color_);
        sprite->SetTextureLeftTop(GetTextureLeftTopFromDigit(digit));
        sprite->SetTextureSize(textureCellSize_);
        sprite->SetSize(digitSize_);
        float x = position_.x + (digitSize_.x + spacing_) * static_cast<float>(i);

        // 4桁表示のとき、左2桁を「分」、右2桁を「秒」として見やすくする
        if (i >= 2) {
            x += middleGap_;
        }

        sprite->SetPos({
            x,
            position_.y
            });
    }
}

void SpriteNumberText::Update()
{
    for (auto& sprite : digitSprites_) {
        sprite->Update();
    }
}

void SpriteNumberText::Draw()
{
    for (auto& sprite : digitSprites_) {
        sprite->Draw();
    }
}