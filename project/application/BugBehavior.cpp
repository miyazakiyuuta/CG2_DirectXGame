#include "BugBehavior.h"
#include "Bug.h"
#include "utility/Random.h"
#include <algorithm>
#include <cmath>

// --- BehaviorStraight ---
void BehaviorStraight::Initialize(Bug* bug) {
	targetPos_ = bug->GetRandomPositionInRange();
	maxSpeed_ = Random::GetFloat(0.08f, 0.15f);
}
void BehaviorStraight::Update(Bug* bug) {
	Vector3 currentPos = bug->GetPosition();
	Vector3 toTarget = {targetPos_.x - currentPos.x, targetPos_.y - currentPos.y, targetPos_.z - currentPos.z};
	float distance = toTarget.Length();

	if (distance > 0.01f) {
		// 到着間近なら減速、それ以外は加速する力を加える（操舵）
		float proximitySlowdown = std::clamp(distance / 2.0f, 0.2f, 1.0f);
		// ★静的関数 Vector3::Normalized を使用するように修正
		Vector3 desiredVelocity = Vector3::Normalized(toTarget) * (maxSpeed_ * proximitySlowdown);
		bug->ApplySteeringForce(desiredVelocity);
	}
}
bool BehaviorStraight::IsFinished(Bug* bug) const {
	Vector3 diff = {bug->GetPosition().x - targetPos_.x, bug->GetPosition().y - targetPos_.y, bug->GetPosition().z - targetPos_.z};
	return diff.Length() < 0.2f;
}

// --- BehaviorHover ---
void BehaviorHover::Initialize(Bug* bug) {
	timer_ = 0.0f;
	duration_ = Random::GetFloat(40.0f, 120.0f);
}
void BehaviorHover::Update(Bug* bug) {
	timer_ += 1.0f;
	// ホバリング中は速度をゼロに向かわせる（自然な摩擦で止まる）
	bug->ApplySteeringForce({0, 0, 0});
}
bool BehaviorHover::IsFinished(Bug* bug) const { return timer_ >= duration_; }

// --- BehaviorCurve ---
void BehaviorCurve::Initialize(Bug* bug) {
	startPos_ = bug->GetPosition();
	endPos_ = bug->GetRandomPositionInRange();
	controlPos_ = bug->GetRandomPositionInRange();
	t_ = 0.0f;
	tSpeed_ = Random::GetFloat(0.005f, 0.015f);
}
void BehaviorCurve::Update(Bug* bug) {
	t_ += tSpeed_;
	float invT = 1.0f - t_;
	// ベジェ曲線上の「次の目標点」を計算
	Vector3 nextPathPos;
	nextPathPos.x = invT * invT * startPos_.x + 2.0f * invT * t_ * controlPos_.x + t_ * t_ * endPos_.x;
	nextPathPos.y = invT * invT * startPos_.y + 2.0f * invT * t_ * controlPos_.y + t_ * t_ * endPos_.y;
	nextPathPos.z = invT * invT * startPos_.z + 2.0f * invT * t_ * controlPos_.z + t_ * t_ * endPos_.z;

	Vector3 toPath = {nextPathPos.x - bug->GetPosition().x, nextPathPos.y - bug->GetPosition().y, nextPathPos.z - bug->GetPosition().z};
	if (toPath.Length() > 0.001f) {
		// ★ここも静的関数 Vector3::Normalized を使用するように修正
		bug->ApplySteeringForce(Vector3::Normalized(toPath) * 0.2f);
	}
}
bool BehaviorCurve::IsFinished(Bug* bug) const { return t_ >= 1.0f; }
