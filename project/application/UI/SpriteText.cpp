#include "UI/SpriteText.h"

#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"

void SpriteText::Initialize(
    SpriteCommon* spriteCommon,
    const std::string& textureFilePath,
    const std::string& atlasCharactersUtf8,
    int atlasCols,
    int maxCharacters
)
{
    spriteCommon_ = spriteCommon;
    textureFilePath_ = textureFilePath;
    atlasCols_ = atlasCols;

    charToIndex_.clear();

    std::vector<char32_t> atlasChars = DecodeUtf8(atlasCharactersUtf8);
    for (int i = 0; i < static_cast<int>(atlasChars.size()); ++i) {
        charToIndex_[atlasChars[i]] = i;
    }

    sprites_.clear();
    sprites_.reserve(maxCharacters);

    for (int i = 0; i < maxCharacters; ++i) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(spriteCommon_, textureFilePath_);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        sprite->SetSize(charSize_);
        sprite->SetTextureSize(cellSize_);
        sprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        sprites_.push_back(std::move(sprite));
    }
}

std::vector<char32_t> SpriteText::DecodeUtf8(const std::string& text) const
{
    std::vector<char32_t> result;

    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        if (c < 0x80) {
            result.push_back(c);
            ++i;
        }
        else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            char32_t code =
                ((text[i] & 0x1F) << 6) |
                (text[i + 1] & 0x3F);
            result.push_back(code);
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            char32_t code =
                ((text[i] & 0x0F) << 12) |
                ((text[i + 1] & 0x3F) << 6) |
                (text[i + 2] & 0x3F);
            result.push_back(code);
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            char32_t code =
                ((text[i] & 0x07) << 18) |
                ((text[i + 1] & 0x3F) << 12) |
                ((text[i + 2] & 0x3F) << 6) |
                (text[i + 3] & 0x3F);
            result.push_back(code);
            i += 4;
        }
        else {
            ++i;
        }
    }

    return result;
}

Vector2 SpriteText::GetTextureLeftTop(char32_t c) const
{
    auto it = charToIndex_.find(c);
    if (it == charToIndex_.end()) {
        return { 0.0f, 0.0f };
    }

    int index = it->second;
    int x = index % atlasCols_;
    int y = index / atlasCols_;

    return {
        cellSize_.x * static_cast<float>(x),
        cellSize_.y * static_cast<float>(y)
    };
}

void SpriteText::SetText(const std::string& textUtf8)
{
    std::vector<char32_t> chars = DecodeUtf8(textUtf8);

    for (size_t i = 0; i < sprites_.size(); ++i) {
        Sprite* sprite = sprites_[i].get();

        if (i >= chars.size()) {
            sprite->SetColor({ color_.x, color_.y, color_.z, 0.0f });
            continue;
        }

        char32_t c = chars[i];

        auto it = charToIndex_.find(c);
        if (it == charToIndex_.end()) {
            sprite->SetColor({ color_.x, color_.y, color_.z, 0.0f });
            continue;
        }

        sprite->SetColor(color_);
        sprite->SetTextureLeftTop(GetTextureLeftTop(c));
        sprite->SetTextureSize(cellSize_);
        sprite->SetSize(charSize_);
        sprite->SetPos({
            position_.x + (charSize_.x + spacing_) * static_cast<float>(i),
            position_.y
            });
    }
}

void SpriteText::Update()
{
    for (auto& sprite : sprites_) {
        sprite->Update();
    }
}

void SpriteText::Draw()
{
    for (auto& sprite : sprites_) {
        sprite->Draw();
    }
}