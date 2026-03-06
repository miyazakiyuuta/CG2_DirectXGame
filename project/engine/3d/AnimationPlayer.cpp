#include "3d/AnimationPlayer.h"
#include "math/mathFunction.h"
#include "math/Matrix4x4.h"
#include <cmath>
#include <cassert>

void AnimationPlayer::Update(float deltaTime, const std::string& nodeName) {
	if (!animation_)return;

	animationTime_ += deltaTime;
	animationTime_ = std::fmod(animationTime_, animation_->duration); // リピート再生※fmod(x,y)=x/yの余り(0に近いほうの整数に丸めた値)

	const NodeAnimation& targetNodeAnimation = animation_->nodeAnimations.at(nodeName);

	Vector3 translate = CalculateValue(targetNodeAnimation.translate, animationTime_);
	Quaternion rotate = CalculateValue(targetNodeAnimation.rotate, animationTime_);
	Vector3 scale = CalculateValue(targetNodeAnimation.scale, animationTime_);
	localMatrix_ = Matrix4x4::Affine(scale, rotate, translate);
}

Vector3 AnimationPlayer::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
	assert(!keyframes.empty()); // キーがないものは返す値がわからないのでダメ
	if (keyframes.size() == 1 || time <= keyframes[0].time) { // キーが1つか、時刻がキーフレーム前なら最初の値とする
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		// indexといnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			// 範囲内を補間する
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
			return Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}
	return (*keyframes.rbegin()).value;
}

Quaternion AnimationPlayer::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
	assert(!keyframes.empty());
	if (keyframes.size() == 1 || time <= keyframes[0].time) {
		return keyframes[0].value;
	}

	for (size_t index = 0; index < keyframes.size() - 1; ++index) {
		size_t nextIndex = index + 1;
		// indexといnextIndexの2つのkeyframeを取得して範囲内に時刻があるかを判定
		if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
			// 範囲内を補間する
			float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
			return Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
		}
	}
	return (*keyframes.rbegin()).value;
}