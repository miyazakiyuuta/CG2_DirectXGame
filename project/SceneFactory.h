#pragma once
#include "AbstractSceneFactory.h"
#include <memory>

class SceneFactory :public AbstractSceneFactory {
public:

	/// <summary>
	/// シーン生成
	/// </summary>
	/// <param name="sceneName">シーン名</param>
	/// <returns>生成したシーン</returns>
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
};

