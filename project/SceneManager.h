#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"

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

	AbstractSceneFactory* sceneFactory_ = nullptr;

	BaseScene* scene_ = nullptr;
	BaseScene* nextScene_ = nullptr;
};

