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

	void StopWithBlend(const Skeleton& skeleton, float blendDuration = 0.2f);

	// 一時停止のコントロール
	void SetPause(bool pause) { isPaused_ = pause; }
	bool GetIsPaused() { return isPaused_; }

	static constexpr float kFPS = 60.0f;

	// 総フレーム数を返す（アニメーションがなければ0）
	int GetTotalFrames() const {
		if (!currentAnimation_) return 0;
		return static_cast<int>(currentAnimation_->duration * kFPS);
	}

	// 現在のフレーム番号を返す
	int GetCurrentFrame() const {
		if (!currentAnimation_) return 0;
		return static_cast<int>(currentTime_ * kFPS);
	}

	// 0.0〜1.0の再生進捗率
	float GetProgress() const {
		if (!currentAnimation_ || currentAnimation_->duration <= 0.0f) return 0.0f;
		return currentTime_ / currentAnimation_->duration;
	}

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

	// フェードアウト用
	bool isFadingOut_ = false;
	float fadeOutDuration_ = 0.0f;
	float fadeOutTimer_ = 0.0f;
	std::vector<QuaternionTransform> fadeOutStartPose_; // 開始時ポーズのスナップショット

};