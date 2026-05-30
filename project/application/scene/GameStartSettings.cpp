#include "scene/GameStartSettings.h"

GameStartSettings::Difficulty GameStartSettings::difficulty_ =
GameStartSettings::Difficulty::Normal;

std::string GameStartSettings::stageFilePath_ =
"resources/stage_normal.json";

void GameStartSettings::SetDifficulty(Difficulty difficulty)
{
	difficulty_ = difficulty;

	switch (difficulty_) {
	case Difficulty::Normal:
		stageFilePath_ = "resources/stage_normal.json";
		break;
	case Difficulty::Hard:
		stageFilePath_ = "resources/stage_hard.json";
		break;
	case Difficulty::Hell:
		stageFilePath_ = "resources/stage_hell.json";
		break;
	default:
		difficulty_ = Difficulty::Normal;
		stageFilePath_ = "resources/stage_normal.json";
		break;
	}
}

GameStartSettings::Difficulty GameStartSettings::GetDifficulty()
{
	return difficulty_;
}

const std::string& GameStartSettings::GetStageFilePath()
{
	return stageFilePath_;
}

const char* GameStartSettings::GetDifficultyName()
{
	switch (difficulty_) {
	case Difficulty::Normal:
		return "Normal";
	case Difficulty::Hard:
		return "Hard";
	case Difficulty::Hell:
		return "Hell";
	default:
		return "Normal";
	}
}