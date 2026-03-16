#include "Bug.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "BugBehavior.h"
#include "utility/Random.h"
#include <algorithm>
#include <cmath>

Bug::Bug() = default;
Bug::~Bug() = default;

void Bug::Initialize(Camera* camera){
	object_ = std::make_unique<Object3d>();
	object_->Initialize(Object3dCommon::GetInstance());
	object_->SetModel("sphere.obj");
	object_->SetCamera(camera);
	object_->SetScale({ 0.2f, 0.2f, 0.2f });

	position_ = { 0.0f, 2.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	currentYaw_ = 0.0f;

	object_->SetTranslate(position_);
	ChangeRandomBehavior();
}

void Bug::Update(){
	// 1. 挙動による操舵
	if(behavior_){
		behavior_->Update(this);
		if(behavior_->IsFinished(this)){
			ChangeRandomBehavior();
		}
	}

	// 2. 慣性と摩擦
	velocity_ = { velocity_.x * 0.92f, velocity_.y * 0.92f, velocity_.z * 0.92f };
	position_ += velocity_;

	// 3. 滑らかな回転
	if(velocity_.Length() > 0.01f){
		float targetYaw = std::atan2(velocity_.x, velocity_.z);

		float diff = targetYaw - currentYaw_;
		while(diff > 3.14159f) diff -= 6.28318f;
		while(diff < -3.14159f) diff += 6.28318f;

		currentYaw_ += diff * 0.1f;
		object_->SetRotate({ 0.0f, currentYaw_, 0.0f });
	}

	// 4. 生物的ゆらぎ
	float moveSpeed = velocity_.Length();
	jitterTimer_ += 0.04f;

	Vector3 drawPosition = position_;
	float bobbingIntensity = 0.15f * (1.0f - (std::min)(moveSpeed * 10.0f, 0.8f));

	drawPosition.x += (std::sin(jitterTimer_ * 1.37f) * 0.12f + std::sin(jitterTimer_ * 2.91f) * 0.04f);
	drawPosition.y += (std::sin(jitterTimer_ * 0.73f) + std::sin(jitterTimer_ * 1.51f) * 0.5f) * bobbingIntensity;
	drawPosition.z += (std::cos(jitterTimer_ * 1.19f) * 0.1f + std::sin(jitterTimer_ * 3.17f) * 0.03f);

	object_->SetTranslate(drawPosition);
	object_->Update();
}

void Bug::ApplySteeringForce(const Vector3& desiredVelocity){
	velocity_.x += (desiredVelocity.x - velocity_.x) * 0.05f;
	velocity_.y += (desiredVelocity.y - velocity_.y) * 0.05f;
	velocity_.z += (desiredVelocity.z - velocity_.z) * 0.05f;
}

void Bug::Draw(){
	if(object_){
		object_->Draw();
	}
}

Vector3 Bug::GetRandomPositionInRange() const{
	return Vector3(
		Random::GetFloat(-moveAreaRadius_, moveAreaRadius_),
		Random::GetFloat(moveAreaHeightMin_, moveAreaHeightMax_),
		Random::GetFloat(-moveAreaRadius_, moveAreaRadius_)
	);
}

void Bug::LookAt(const Vector3& direction){
	(void)direction;
}

void Bug::ChangeRandomBehavior(){
	int r = Random::GetInt(0, 2);
	if(r == 0){
		behavior_ = std::make_unique<BehaviorStraight>();
	} else if(r == 1){
		behavior_ = std::make_unique<BehaviorHover>();
	} else{
		behavior_ = std::make_unique<BehaviorCurve>();
	}

	behavior_->Initialize(this);
}

CollisionUtility::Sphere Bug::GetHitSphere() const{
	CollisionUtility::Sphere sphere;
	sphere.center = position_;
	sphere.radius = hitRadius_;
	return sphere;
}

void Bug::OnTongueHit(){
	position_ = GetRandomPositionInRange();
	velocity_ = { 0.0f, 0.0f, 0.0f };
	ChangeRandomBehavior();

	if(object_){
		object_->SetTranslate(position_);
		object_->Update();
	}
}