#pragma once
#include "3d/Animation.h"
#include "math/Matrix4x4.h"

class AnimationPlayer {
public:
	void Update(float deltaTime, const std::string& nodeName);

	static Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
	static Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

	void SetAnimation(const Animation* animation) { animation_ = animation; }

	Matrix4x4 GetLocalMatrix()const { return localMatrix_; }

private:
    const Animation* animation_ = nullptr;
	float animationTime_ = 0.0f; // 再生中の時刻
	Matrix4x4 localMatrix_;

};