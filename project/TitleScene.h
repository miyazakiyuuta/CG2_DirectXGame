#pragma once
#include "BaseScene.h"


class TitleScene :public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;

	void Update() override;

	void Draw() override;
};

