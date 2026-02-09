#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include <memory>

class SceneManager {
public:
	static SceneManager* GetInstance();

	void Update();

	void Draw();

	~SceneManager();

	void ChangeScene(const std::string& sceneName);

	void SetSceneFactory(AbstractSceneFactory* factory) { sceneFactory_ = factory; }

private:
	static SceneManager* instance;

	AbstractSceneFactory* sceneFactory_;

	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
};

