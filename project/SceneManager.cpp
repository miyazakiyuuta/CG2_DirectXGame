#include "SceneManager.h"
#include <cassert>

SceneManager* SceneManager::instance = nullptr;
SceneManager* SceneManager::GetInstance() {
	if (!instance)instance = new SceneManager();
	return instance;
}

void SceneManager::Update() {
	// 次シーンの予約があるなら
	if (nextScene_) {
		// 旧シーンの終了
		if (scene_) {
			scene_->Finalize();
			delete scene_;
		}

		// シーン切り替え
		scene_ = nextScene_;
		nextScene_ = nullptr;

		// 次シーンを初期化する
		scene_->Initialize();
	}


	// 実行中シーンを更新する
	scene_->Update();
}

void SceneManager::Draw() {
	scene_->Draw();
}

SceneManager::~SceneManager() {
	// 最期のシーンの終了と解放
	scene_->Finalize();
	delete scene_;
}

void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);

	nextScene_ = sceneFactory_->CreateScene(sceneName);
}