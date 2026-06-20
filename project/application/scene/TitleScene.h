#pragma once
#include "scene/BaseScene.h"


class TitleScene :public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;
};

