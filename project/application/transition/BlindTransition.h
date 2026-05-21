#pragma once
#include "scene/ITransition.h"

#include <memory>
#include <vector>

class Sprite;

class BlindTransition : public ITransition {
public:
	BlindTransition();

	void Initialize() override;
	void Update() override;
	void Draw() override;

	bool IsReadyToChange() const override { return isClosed_; }
	bool IsFinished() const override { return isOpened_; }

private:
	float windowHeight_;
	bool isClosed_;
	bool isOpened_;

	static constexpr int kStripCount = 8;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	float fadeSpeed_ = 0.025f;
	float timer_ = 0.0f;
	float stripDelay_ = 0.1f;
};

