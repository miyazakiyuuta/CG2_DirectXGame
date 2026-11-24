#include "scene/SceneFactory.h"
#include "scene/BaseScene.h"
#include "scene/TitleScene.h"
#include "scene/GamePlayScene.h"
#include "scene/ClearResultScene.h"
#include "scene/GameOverResultScene.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	std::unique_ptr<BaseScene> newScene = nullptr;

	if (sceneName == "TITLE") {
		newScene = std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		newScene = std::make_unique<GamePlayScene>();
	} else if (sceneName == "CLEAR") {
		// 結果表示はResultSceneの派生クラスで作り分ける(違いは派生クラスが持つので、SceneManagerは
		// 「結果の種別」という概念を知らずに、基底のポインタ(BaseScene)のまま扱える)
		newScene = std::make_unique<ClearResultScene>();
	} else if (sceneName == "GAMEOVER") {
		newScene = std::make_unique<GameOverResultScene>();
	}

	return newScene;
}
