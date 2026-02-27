#pragma once
#include "scene/ITransition.h"
#include <memory>

class Sprite;

class ShutterTransition : public ITransition {
public:
	ShutterTransition();

	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsReadyToChange() const override { return isClosed_; }
	bool IsFinished() const override { return isOpened_; }

private:
	float shutterHeight_;
	bool isClosed_;
	bool isOpened_;

	std::unique_ptr<Sprite> sprite_;
};