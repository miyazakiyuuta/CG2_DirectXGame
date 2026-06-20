#include "SceneManager.h"
#include "effect/EffectManager.h"
#include <cassert>

SceneManager* SceneManager::instance = nullptr;
SceneManager* SceneManager::GetInstance() {
	if (!instance)instance = new SceneManager();
	return instance;
}

void SceneManager::Update(float deltaTime) {
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

	if (scene_) { scene_->Update(deltaTime); }
} 

void SceneManager::Draw() {
	if (scene_) { 
		scene_->Draw(); }
	if (transition_) { 
		transition_->Draw();
	}
}

void SceneManager::DrawImGui() {
	if (scene_) {
		scene_->DrawImGui();
	}
}

SceneManager::~SceneManager() {
	// 最期のシーンの終了と解放
	scene_->Finalize();
}

void SceneManager::ChangeScene(const std::string& sceneName, std::unique_ptr<ITransition> transition) {
	assert(sceneFactory_);
	if (nextScene_ != nullptr || transition_ != nullptr) {
		return;
	}
	assert(nextScene_ == nullptr);

	effectManager_->ResetAll();
	nextScene_ = sceneFactory_->CreateScene(sceneName);
	nextScene_->SetEffectManager(effectManager_);
	transition_ = std::move(transition);
	if (transition_) { transition_->Initialize(); }
	isSceneChanged_ = false;
}