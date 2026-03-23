#pragma once
#include "3d/Animation.h"
#include "3d/Skeleton.h"
#include "math/Matrix4x4.h"

class AnimationPlayer {
public:
	void Update(float deltaTime, Skeleton& skeleton);

	void Play(const Animation* animation, bool isLoop, float blendDuration);

	void Restart() { currentTime_ = 0.0f; }

	bool IsFinished() const {
		if (currentIsLoop_ || !currentAnimation_) return false;
		return currentTime_ >= currentAnimation_->duration;
	}

	// 完全停止、アニメーションデータもクリア
	void Stop() {
		currentAnimation_ = nullptr;
		previousAnimation_ = nullptr;
		isBlending_ = false;
	}

	// 一時停止のコントロール
	void SetPause(bool pause) { isPaused_ = pause; }
	bool GetIsPaused() { return isPaused_; }

private:
	static Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
	static Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

private:
	// 現在のアニメーション
    const Animation* currentAnimation_ = nullptr;
	float currentTime_ = 0.0f; // 再生中の時刻
	bool currentIsLoop_ = true;

	// 遷移前のアニメーション(ブレンド用)
	const Animation* previousAnimation_ = nullptr;
	float previousTime_ = 0.0f;

	// ブレンド用変数
	float blendDuration_ = 0.0f; // ブレンドにかける合計時間
	float blendTimer_ = 0.0f; // 現在の経過時間
	bool isBlending_ = false;

	bool isPaused_ = false; // 一時停止フラグ
};