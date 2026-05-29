#pragma once

#include "math/Vector2.h"
#include "2d/Sprite.h"
#include "UI/SpriteNumberText.h"

#include <memory>
#include <vector>

class SpriteCommon;

class HeightProgressMeter {
public:
	void Initialize(SpriteCommon* spriteCommon, float goalHeight);
	void Update(float currentHeight, float goalHeight);
	void Draw();

private:
	void UpdateLayout();
	int CountDigits(int value) const;

private:
	SpriteCommon* spriteCommon_ = nullptr;

	std::unique_ptr<Sprite> backSprite_ = nullptr;
	std::unique_ptr<Sprite> fillSprite_ = nullptr;
	std::unique_ptr<Sprite> markerSprite_ = nullptr;
	std::vector<std::unique_ptr<Sprite>> tickSprites_;

	SpriteNumberText zeroText_;
	SpriteNumberText goalText_;
	SpriteNumberText currentText_;

	float currentHeight_ = 0.0f;
	float goalHeight_ = 1.0f;

	Vector2 meterSize_ = { 18.0f, 260.0f };
	Vector2 labelDigitSize_ = { 18.0f, 22.0f };
	Vector2 currentDigitSize_ = { 16.0f, 20.0f };

	float rightMargin_ = 26.0f;
	float bottomMargin_ = 230.0f;

	float majorTickLength_ = 28.0f;
	float minorTickLength_ = 14.0f;
	float tickThickness_ = 2.0f;

	bool initialized_ = false;
};