#pragma once

#include "math/Vector4.h"
#include <memory>
#include <string>
#include <vector>

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
	bool IsRestartRequested() const { return isRestartRequested_; }

private:
	std::unique_ptr<Sprite> CreateTextSprite(const std::string& textUtf8, const std::string& outputPath, float yPos, const Vector4& color, int fontSize);
	std::vector<float> LoadClearTimeRanking() const;
	bool SaveClearTimeRanking(const std::vector<float>& times) const;
	bool UpdateClearTimeRanking(float clearTimeSeconds);
	std::string FormatClearTime(float seconds) const;
	void CreateClearRankingSprites(float clearTimeSeconds);

private:
	SpriteCommon* spriteCommon_ = nullptr;
	State state_ = State::None;

	std::unique_ptr<Sprite> bgSprite_ = nullptr;
	std::unique_ptr<Sprite> textTitleSprite_ = nullptr;
	std::unique_ptr<Sprite> textTimeSprite_ = nullptr;
	std::unique_ptr<Sprite> textGuidanceSprite_ = nullptr;
	std::unique_ptr<Sprite> textGuidanceGamepadSprite_ = nullptr;
	std::unique_ptr<Sprite> textRestartGuidanceSprite_ = nullptr;
	std::unique_ptr<Sprite> textRestartGuidanceGamepadSprite_ = nullptr;

	float effectAlpha_ = 0.0f;
	bool isTitleRequested_ = false;
	bool isRestartRequested_ = false;
	bool isGamepadMode_ = false;

	static constexpr int kClearTimeRankingMaxCount_ = 5;
	static constexpr const char* kClearTimeRankingFilePath_ =
		"resources/clear_time_ranking.json";

	std::vector<float> clearTimeRanking_;
	bool clearTimeRankingUpdated_ = false;
	int clearTimeRankingInsertedIndex_ = -1;
	float clearTimeBlinkTimer_ = 0.0f;

	std::unique_ptr<Sprite> rankingTitleSprite_ = nullptr;
	std::vector<std::unique_ptr<Sprite>> rankingTimeSprites_;

	std::unique_ptr<Sprite> currentTimeLabelSprite_ = nullptr;
	std::unique_ptr<Sprite> currentTimeValueSprite_ = nullptr;
};