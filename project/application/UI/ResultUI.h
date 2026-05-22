#pragma once

#include "math/Vector4.h"
#include <memory>
#include <string>

class Sprite;
class SpriteCommon;

// クリア・ゲームオーバー時の演出UIクラス
class ResultUI {
public:
	enum class State { None, Clear, GameOver };

	void Initialize(SpriteCommon* spriteCommon);
	void Update();
	void Draw();

	void TriggerClear(float clearTimeSeconds);
	void TriggerGameOver();

	bool IsActive() const { return state_ != State::None; }
	bool IsTitleRequested() const { return isTitleRequested_; }

private:
	std::unique_ptr<Sprite> CreateTextSprite(const std::string& textUtf8, const std::string& outputPath, float yPos, const Vector4& color, int fontSize);

private:
	SpriteCommon* spriteCommon_ = nullptr;
	State state_ = State::None;

	std::unique_ptr<Sprite> bgSprite_ = nullptr;
	std::unique_ptr<Sprite> textTitleSprite_ = nullptr;
	std::unique_ptr<Sprite> textTimeSprite_ = nullptr;
	std::unique_ptr<Sprite> textGuidanceSprite_ = nullptr;

	float effectAlpha_ = 0.0f;
	bool isTitleRequested_ = false;
};