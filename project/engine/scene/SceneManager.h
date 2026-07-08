#pragma once
#include "scene/BaseScene.h"
#include "scene/AbstractSceneFactory.h"
#include "scene/ITransition.h"
#include <memory>

class EffectManager;

class SceneManager {
public:
	static SceneManager* GetInstance();
	// 最終シーンの終了処理とシングルトンの破棄(Framework::Finalizeが呼ぶ)
	static void Finalize();

	void Update(float deltaTime);

	void Draw();

	void DrawImGui();

	void ChangeScene(const std::string& sceneName, std::unique_ptr<ITransition> transition = nullptr);

	void SetSceneFactory(AbstractSceneFactory* factory) { sceneFactory_ = factory; }

	void SetEffectManager(EffectManager* effectManager) {
		effectManager_ = effectManager;
		if (scene_) {
			scene_->SetEffectManager(effectManager);
		}
		if (nextScene_) {
			nextScene_->SetEffectManager(effectManager);
		}
	}

private:
	static SceneManager* instance;

	AbstractSceneFactory* sceneFactory_;

	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;

	std::unique_ptr<ITransition> transition_ = nullptr;
	bool isSceneChanged_ = false;

	EffectManager* effectManager_ = nullptr;
};

