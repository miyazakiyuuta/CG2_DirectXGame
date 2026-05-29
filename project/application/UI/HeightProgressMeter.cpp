#define NOMINMAX
#include "UI/HeightProgressMeter.h"

#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "base/WinApp.h"

#include <algorithm>
#include <cmath>
#include <string>

int HeightProgressMeter::CountDigits(int value) const
{
	value = std::abs(value);

	int digits = 1;
	while (value >= 10) {
		value /= 10;
		++digits;
	}

	return digits;
}

void HeightProgressMeter::Initialize(SpriteCommon* spriteCommon, float goalHeight)
{
	spriteCommon_ = spriteCommon;
	goalHeight_ = std::max(1.0f, goalHeight);
	currentHeight_ = 0.0f;

	if (!spriteCommon_) {
		return;
	}

	TextureManager::GetInstance()->LoadTexture(TextureManager::kDefaultTextureName);
	TextureManager::GetInstance()->LoadTexture("resources/UI/KiwiMaruNumStrength.png");

	backSprite_ = std::make_unique<Sprite>();
	backSprite_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	backSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	backSprite_->SetColor({ 0.04f, 0.06f, 0.08f, 0.72f });

	fillSprite_ = std::make_unique<Sprite>();
	fillSprite_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	fillSprite_->SetAnchorPoint({ 0.5f, 1.0f });
	fillSprite_->SetColor({ 0.25f, 0.75f, 1.0f, 0.55f });

	markerSprite_ = std::make_unique<Sprite>();
	markerSprite_->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
	markerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	markerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });

	tickSprites_.clear();
	tickSprites_.reserve(11);

	for (int i = 0; i <= 10; ++i) {
		const bool isMajorTick = i == 0 || i == 5 || i == 10;

		auto tick = std::make_unique<Sprite>();
		tick->Initialize(spriteCommon_, TextureManager::kDefaultTextureName);
		tick->SetAnchorPoint({ 1.0f, 0.5f });
		tick->SetColor({ 1.0f, 1.0f, 1.0f, isMajorTick ? 0.9f : 0.55f });

		tickSprites_.push_back(std::move(tick));
	}

	zeroText_.Initialize(
		spriteCommon_,
		"resources/UI/KiwiMaruNumStrength.png",
		1
	);
	zeroText_.SetDigitSize(labelDigitSize_);
	zeroText_.SetSpacing(0.0f);
	zeroText_.SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });
	zeroText_.SetNumber(0, 1);

	const int goalValue = static_cast<int>(goalHeight_ + 0.5f);
	const int goalDigits = CountDigits(goalValue);

	goalText_.Initialize(
		spriteCommon_,
		"resources/UI/KiwiMaruNumStrength.png",
		goalDigits
	);
	goalText_.SetDigitSize(labelDigitSize_);
	goalText_.SetSpacing(0.0f);
	goalText_.SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });
	goalText_.SetNumber(goalValue, goalDigits);

	currentText_.Initialize(
		spriteCommon_,
		"resources/UI/KiwiMaruNumStrength.png",
		4
	);
	currentText_.SetDigitSize(currentDigitSize_);
	currentText_.SetSpacing(0.0f);
	currentText_.SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });
	currentText_.SetNumber(0, 1);

	initialized_ = true;

	UpdateLayout();
}

void HeightProgressMeter::Update(float currentHeight, float goalHeight)
{
	if (!initialized_) {
		return;
	}

	currentHeight_ = currentHeight;
	goalHeight_ = std::max(1.0f, goalHeight);

	UpdateLayout();
}

void HeightProgressMeter::UpdateLayout()
{
	const float screenW = static_cast<float>(WinApp::kClientWidth);
	const float screenH = static_cast<float>(WinApp::kClientHeight);

	const float barRight = screenW - rightMargin_;
	const float barCenterX = barRight - meterSize_.x * 0.5f;

	const float barBottomY = screenH - bottomMargin_;
	const float barTopY = barBottomY - meterSize_.y;
	const float barCenterY = (barTopY + barBottomY) * 0.5f;

	float progress = 0.0f;
	if (goalHeight_ > 0.0001f) {
		progress = currentHeight_ / goalHeight_;
	}

	progress = std::clamp(progress, 0.0f, 1.0f);

	if (backSprite_) {
		backSprite_->SetPos({ barCenterX, barCenterY });
		backSprite_->SetSize(meterSize_);
		backSprite_->Update();
	}

	if (fillSprite_) {
		fillSprite_->SetPos({ barCenterX, barBottomY });
		fillSprite_->SetSize({
			meterSize_.x,
			meterSize_.y * progress
			});
		fillSprite_->Update();
	}

	const float markerY = barBottomY - meterSize_.y * progress;

	if (markerSprite_) {
		markerSprite_->SetPos({ barCenterX, markerY });
		markerSprite_->SetSize({ meterSize_.x + 12.0f, 4.0f });
		markerSprite_->Update();
	}

	int currentValue = static_cast<int>(currentHeight_ + 0.5f);
	currentValue = std::clamp(currentValue, 0, 9999);

	const int currentDigits = CountDigits(currentValue);

	currentText_.SetPosition({
		barCenterX - meterSize_.x * 0.5f - majorTickLength_ - 78.0f,
		markerY - currentDigitSize_.y * 0.5f
		});
	currentText_.SetNumber(currentValue, currentDigits);
	currentText_.Update();

	for (int i = 0; i <= 10 && i < static_cast<int>(tickSprites_.size()); ++i) {
		const bool isMajorTick = i == 0 || i == 5 || i == 10;

		const float tickLength =
			isMajorTick ? majorTickLength_ : minorTickLength_;

		const float y =
			barBottomY - meterSize_.y * (static_cast<float>(i) / 10.0f);

		tickSprites_[i]->SetPos({
			barCenterX - meterSize_.x * 0.5f - 4.0f,
			y
			});

		tickSprites_[i]->SetSize({
			tickLength,
			tickThickness_
			});

		tickSprites_[i]->Update();
	}

	zeroText_.SetPosition({
		barCenterX - meterSize_.x * 0.5f - majorTickLength_ - 42.0f,
		barBottomY - labelDigitSize_.y * 0.5f
		});
	zeroText_.SetNumber(0, 1);
	zeroText_.Update();

	const int goalValue = static_cast<int>(goalHeight_ + 0.5f);
	const int goalDigits = CountDigits(goalValue);

	goalText_.SetPosition({
		barCenterX - meterSize_.x * 0.5f - majorTickLength_ - 96.0f,
		barTopY - labelDigitSize_.y * 0.5f
		});
	goalText_.SetNumber(goalValue, goalDigits);
	goalText_.Update();
}

void HeightProgressMeter::Draw()
{
	if (!initialized_) {
		return;
	}

	if (backSprite_) {
		backSprite_->Draw();
	}

	if (fillSprite_) {
		fillSprite_->Draw();
	}

	for (auto& tick : tickSprites_) {
		if (tick) {
			tick->Draw();
		}
	}

	if (markerSprite_) {
		markerSprite_->Draw();
	}

	zeroText_.Draw();
	goalText_.Draw();
	currentText_.Draw();
}