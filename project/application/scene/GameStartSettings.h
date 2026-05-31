#pragma once

#include <string>

class GameStartSettings {
public:
	enum class Difficulty {
		Normal,
		Hard,
		Hell,
	};

	static void SetDifficulty(Difficulty difficulty);
	static Difficulty GetDifficulty();

	static const std::string& GetStageFilePath();
	static const char* GetDifficultyName();
	static float GetXPMultiplier();

private:
	static Difficulty difficulty_;
	static std::string stageFilePath_;
};