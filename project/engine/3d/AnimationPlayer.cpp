#include "3d/AnimationPlayer.h"
#include "math/mathFunction.h"
#include "math/Matrix4x4.h"
#include <cmath>
#include <cassert>

void AnimationPlayer::Update(float deltaTime, Skeleton& skeleton) {
	// フェードアウト中はデフォルトポーズへ補間する
	if (isFadingOut_) {
		fadeOutTimer_ += deltaTime;
		float t = std::min(fadeOutTimer_ / fadeOutDuration_, 1.0f);

		for (size_t i = 0; i < skeleton.joints.size(); ++i) {
			Joint& joint = skeleton.joints[i];
			const QuaternionTransform& start = fadeOutStartPose_[i];
			const QuaternionTransform& rest = skeleton.restPoseTransforms[i];

			joint.transform.translate = Lerp(start.translate, rest.translate, t);
			joint.transform.rotate = Slerp(start.rotate, rest.rotate, t);
			joint.transform.scale = Lerp(start.scale, rest.scale, t);
		}

		if (fadeOutTimer_ >= fadeOutDuration_) {
			isFadingOut_ = false;
			// ★ 補足: フェードアウト完了後、restPoseTransformsをジョイントに書き戻す
			//    これにより UpdateSkeleton→Model::Update でpalette=Identity相当になる
			for (size_t i = 0; i < skeleton.joints.size(); ++i) {
				skeleton.joints[i].transform = skeleton.restPoseTransforms[i];
			}
		}
		return; // 通常アニメーション処理はスキップ
	}

	if (!currentAnimation_ || isPaused_)return;

	currentTime_ += deltaTime;
	if (currentIsLoop_) {
		currentTime_ = std::fmod(currentTime_, currentAnimation_->duration); // ※fmod(x,y)=x/yの余り(0に近いほうの整数に丸めた値)
	} else {
		currentTime_ = (std::min)(currentTime_, currentAnimation_->duration);
	}

	if (isBlending_) {
		blendTimer_ += deltaTime;
		previousTime_ += deltaTime; // 前のアニメーションも時間を進めておく
		if (blendTimer_ >= blendDuration_) {
			isBlending_ = false;
			previousAnimation_ = nullptr;
		}
	}


	float t = isBlending_ ? (blendTimer_ / blendDuration_) : 1.0f; // isBlendingがtrueならblendTimer_ / blendDuration_, falseなら1.0fを返す

	for (Joint& joint : skeleton.joints) {

		// 現在のアニメーション値を計算
		Vector3 curT = joint.transform.translate;
		Quaternion curR = joint.transform.rotate;
		Vector3 curS = joint.transform.scale;

		if (auto it = currentAnimation_->nodeAnimations.find(joint.name); it != currentAnimation_->nodeAnimations.end()) {
			curT = CalculateValue(it->second.translate, currentTime_);
			curR = CalculateValue(it->second.rotate, currentTime_);
			curS = CalculateValue(it->second.scale, currentTime_);
		}

		if (isBlending_ && previousAnimation_) {
			// 前のアニメーション値も計算して混ぜる
			if (auto it = previousAnimation_->nodeAnimations.find(joint.name); it != previousAnimation_->nodeAnimations.end()) {
				Vector3 preT = CalculateValue(it->second.translate, previousTime_);
				Quaternion preR = CalculateValue(it->second.rotate, previousTime_);
				Vector3 preS = CalculateValue(it->second.scale, previousTime_);

				// 補間
				joint.transform.translate = Lerp(preT, curT, t);
				joint.transform.rotate = Slerp(preR, curR, t);
				joint.transform.scale = Lerp(preS, curS, t);
			}
		} else { // ブレンド中でない
			joint.transform.translate = curT;
			joint.transform.rotate = curR;
			joint.transform.scale = curS;
		}
	}
}

void AnimationPlayer::Play(const Animation* animation, bool isLoop, float blendDuration) {
	if (IsFinished()) {
		blendDuration = 0.0f;
	}

	// 同じアニメーションが既に再生中なら何もしない
	if (currentAnimation_ == animation && !IsFinished()) return;

	// 今のアニメーションを「前のポーズ」に回す(「過去のもの」にする)
	previousAnimation_ = currentAnimation_;
	previousTime_ = currentTime_;

	// 新しいアニメーションをセット
	currentAnimation_ = animation;
	currentTime_ = 0.0f;
	currentIsLoop_ = isLoop;

	// ブレンド設定
	blendDuration_ = blendDuration;
	blendTimer_ = 0.0f;
	isBlending_ = (previousAnimation_ != nullptr && blendDuration > 0.0f);
}

void AnimationPlayer::StopWithBlend(const Skeleton& skeleton, float blendDuration) {
	if (!currentAnimation_ && !isFadingOut_) return;

	// 呼び出し時点の各ジョイントポーズをスナップショット保存
	fadeOutStartPose_.clear();
	for (const Joint& joint : skeleton.joints) {
		fadeOutStartPose_.push_back(joint.transform);
	}

	currentAnimation_ = nullptr;
	previousAnimation_ = nullptr;
	isBlending_ = false;

	isFadingOut_ = true;
	fadeOutDuration_ = (blendDuration > 0.0f) ? blendDuration : 0.001f;
	fadeOutTimer_ = 0.0f;
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