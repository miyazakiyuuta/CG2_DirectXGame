#pragma once

#include <memory>
#include <string>
#include <vector>

#include "math/Vector2.h"
#include "math/Vector4.h"

class Sprite;
class SpriteCommon;

class SpriteNumberText {
public:
    void Initialize(
        SpriteCommon* spriteCommon,
        const std::string& textureFilePath,
        int maxDigits
    );

    void SetPosition(const Vector2& position) { position_ = position; }
    void SetDigitSize(const Vector2& size) { digitSize_ = size; }
    void SetSpacing(float spacing) { spacing_ = spacing; }
    void SetMiddleGap(float gap) { middleGap_ = gap; }
    void SetColor(const Vector4& color) { color_ = color; }

    void SetNumber(int value, int minDigits = 1);

    void Update();
    void Draw();

private:
    Vector2 GetTextureLeftTopFromDigit(int digit) const;

private:
    SpriteCommon* spriteCommon_ = nullptr;
    std::string textureFilePath_;

    std::vector<std::unique_ptr<Sprite>> digitSprites_;

    Vector2 position_ = { 40.0f, 40.0f };
    Vector2 digitSize_ = { 32.0f, 32.0f };
    Vector2 textureCellSize_ = { 32.0f, 32.0f };

    float spacing_ = 0.0f;
    float middleGap_ = 0.0f;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    std::string currentText_;
};