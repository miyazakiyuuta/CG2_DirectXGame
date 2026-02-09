#pragma once
#include "BaseScene.h"
#include <string>
#include <memory>

/// <summary>
/// シーン工場(概念)
/// </summary>
class AbstractSceneFactory {
public:

	virtual ~AbstractSceneFactory() = default;

	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
};