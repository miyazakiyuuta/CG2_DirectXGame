#include "scene/GameOverResultScene.h"

std::string GameOverResultScene::GetResultTexturePath() const {
	return "resources/ui/gameover.png";
}

Vector4 GameOverResultScene::GetResultColor() const {
	return { 1.0f, 0.45f, 0.45f, 1.0f };
}

std::string GameOverResultScene::GetResultSoundPath() const {
	return "resources/sounds/gameover.mp3";
}

ResultScene::Layout GameOverResultScene::GetLayout() const {
	Layout layout;
	layout.resultY = 0.42f;
	layout.scoreTop = 0.55f;
	layout.menuTopY = 0.70f;
	return layout;
}
