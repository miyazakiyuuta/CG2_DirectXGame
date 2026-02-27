#include "SceneManager.h"
#include <cassert>

SceneManager* SceneManager::instance = nullptr;
SceneManager* SceneManager::GetInstance() {
	if (!instance)instance = new SceneManager();
	return instance;
}

void SceneManager::Update() {
	if (transition_) {
		transition_->Update();
		if (!isSceneChanged_ && transition_->IsReadyToChange()) {
			if (scene_) { scene_->Finalize(); }

			scene_ = std::move(nextScene_);
			nextScene_ = nullptr;

			scene_->Initialize();
			isSceneChanged_ = true;
		}

		if (transition_->IsFinished()) {
			transition_ = nullptr;
			isSceneChanged_ = false;
		}
	} else {
		// 次シーンの予約があるなら
		if (nextScene_) {
			// 旧シーンの終了
			if (scene_) { scene_->Finalize(); }
			// シーン切り替え
			scene_ = std::move(nextScene_);
			nextScene_ = nullptr;
			// 次シーンを初期化する
			scene_->Initialize();
		}
	}

	if (scene_) { scene_->Update(); }
}

void SceneManager::Draw() {
	if (scene_) { 
		scene_->Draw(); }
	if (transition_) { 
		transition_->Draw();
	}
}

SceneManager::~SceneManager() {
	// 最期のシーンの終了と解放
	scene_->Finalize();
}

void SceneManager::ChangeScene(const std::string& sceneName, std::unique_ptr<ITransition> transition) {
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);

	nextScene_ = sceneFactory_->CreateScene(sceneName);
	transition_ = std::move(transition);
	if (transition_) { transition_->Initialize(); }
	isSceneChanged_ = false;
}