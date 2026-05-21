#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "math/Vector2.h"
#include "math/Vector4.h"

class Sprite;
class SpriteCommon;

class SpriteText {
public:
    void Initialize(
        SpriteCommon* spriteCommon,
        const std::string& textureFilePath,
        const std::string& atlasCharactersUtf8,
        int atlasCols,
        int maxCharacters
    );

    void SetPosition(const Vector2& position) { position_ = position; }
    void SetCellSize(const Vector2& size) { cellSize_ = size; }
    void SetCharSize(const Vector2& size) { charSize_ = size; }
    void SetSpacing(float spacing) { spacing_ = spacing; }
    void SetColor(const Vector4& color) { color_ = color; }

    void SetText(const std::string& textUtf8);

    void Update();
    void Draw();

private:
    std::vector<char32_t> DecodeUtf8(const std::string& text) const;
    Vector2 GetTextureLeftTop(char32_t c) const;

private:
    SpriteCommon* spriteCommon_ = nullptr;
    std::string textureFilePath_;

    std::vector<std::unique_ptr<Sprite>> sprites_;

    std::unordered_map<char32_t, int> charToIndex_;

    Vector2 position_ = { 40.0f, 40.0f };
    Vector2 cellSize_ = { 32.0f, 32.0f };
    Vector2 charSize_ = { 32.0f, 32.0f };

    float spacing_ = 0.0f;
    int atlasCols_ = 10;

    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};