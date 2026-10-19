#pragma once
#include "scene/ITransition.h"

#include <memory>
#include <vector>

class Sprite;

class BlindTransition : public ITransition {
public:
	// 遷移を始めるときの帯の状態
	enum class StartState {
		Open,   // 透明から始め、帯を閉じてからシーンを切り替える(従来の動き)
		Closed, // 閉じ切った(真っ黒の)状態から始め、すぐにシーンを切り替えて開く。
		        // 呼び出し側のシーンが自前の演出で画面を真っ黒にしてから使う
	};

	explicit BlindTransition(StartState startState = StartState::Open);

	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;

	bool IsReadyToChange() const override { return isClosed_; }
	bool IsFinished() const override { return isOpened_; }

private:
	float windowHeight_;
	bool isClosed_;
	bool isOpened_;

	static constexpr int kStripCount = 8;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	float fadeSpeed_ = 1.5f; // アルファ変化量[/秒](旧: 0.025/フレーム × 60fps)
	float timer_ = 0.0f;
	float stripDelay_ = 0.1f;
};

