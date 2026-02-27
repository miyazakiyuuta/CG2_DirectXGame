#pragma once
#include "scene/BaseScene.h"
#include "scene/AbstractSceneFactory.h"
#include "scene/ITransition.h"
#include <memory>

class SceneManager {
public:
	static SceneManager* GetInstance();

	void Update();

	void Draw();

	~SceneManager();

	void ChangeScene(const std::string& sceneName, std::unique_ptr<ITransition> transition = nullptr);

	void SetSceneFactory(AbstractSceneFactory* factory) { sceneFactory_ = factory; }

private:
	static SceneManager* instance;

	AbstractSceneFactory* sceneFactory_;

	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;

	std::unique_ptr<ITransition> transition_ = nullptr;
	bool isSceneChanged_ = false;
};

