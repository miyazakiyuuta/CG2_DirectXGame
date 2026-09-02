#include "scene/SceneFactory.h"
#include "scene/BaseScene.h"
#include "scene/TitleScene.h"
#include "scene/GamePlayScene.h"
#include "scene/ResultScene.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	std::unique_ptr<BaseScene> newScene = nullptr;

	if (sceneName == "TITLE") {
		newScene = std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		newScene = std::make_unique<GamePlayScene>();
	} else if (sceneName == "CLEAR") {
		// 結果表示はResultScene 1クラス。シーン名でTypeを作り分けることで、
		// SceneManagerに「結果の種別」を渡す仕組みを足さずに済む
		newScene = std::make_unique<ResultScene>(ResultScene::Type::Clear);
	} else if (sceneName == "GAMEOVER") {
		newScene = std::make_unique<ResultScene>(ResultScene::Type::GameOver);
	}

	return newScene;
}
