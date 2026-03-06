#include "Tongue.h"

#include "Player.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Camera.h"
#include <cmath>

namespace{
	float Length(const Vector3& v){
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	Vector3 Normalize(const Vector3& v){
		float len = Length(v);
		if(len <= 0.0001f){
			return { 0.0f, 0.0f, 1.0f };
		}
		return { v.x / len, v.y / len, v.z / len };
	}
}

void Tongue::Initialize(
	Object3dCommon* object3dCommon,
	Camera* camera,
	Player* owner,
	const std::string& modelName
){
	owner_ = owner;
	camera_ = camera;

	object_ = std::make_unique<Object3d>();
	object_->Initialize(object3dCommon);
	object_->SetModel(modelName);
	object_->SetCamera(camera_);
	object_->SetScale({ 0.3f,0.3f,0.3f });

	Reset();
}

void Tongue::Update(float deltaTime){
	if(!object_ || !owner_){
		return;
	}

	switch(state_){
		case State::Idle:
			UpdateIdle();
			break;
		case State::Extending:
			UpdateExtending(deltaTime);
			break;
		case State::Returning:
			UpdateReturning(deltaTime);
			break;
	}

	object_->SetTranslate(worldPosition_);
	object_->Update();
}

void Tongue::Draw(){
	if(object_){
		object_->Draw();
	}
}

void Tongue::Shot(){
	if(!owner_ || state_ != State::Idle){
		return;
	}

	float yaw = owner_->GetYaw();

	shotDirection_ = {
		std::sin(yaw),
		0.0f,
		std::cos(yaw)
	};

	shotDirection_ = Normalize(shotDirection_);

	shotStartPosition_ = GetMouthWorldPosition();
	worldPosition_ = shotStartPosition_;
	currentDistance_ = 0.0f;

	state_ = State::Extending;

	float tongueYaw = std::atan2(shotDirection_.x, shotDirection_.z);
	object_->SetRotate({ 0.0f, tongueYaw, 0.0f });
}

void Tongue::Reset(){
	state_ = State::Idle;
	currentDistance_ = 0.0f;
	shotDirection_ = { 0.0f, 0.0f, 1.0f };
	shotStartPosition_ = {};
	worldPosition_ = GetMouthWorldPosition();

	if(object_){
		object_->SetTranslate(worldPosition_);
		object_->SetRotate({ 0.0f, 0.0f, 0.0f });
		object_->Update();
	}
}

void Tongue::UpdateIdle(){
	worldPosition_ = GetMouthWorldPosition();

	if(owner_){
		float yaw = owner_->GetYaw();
		object_->SetRotate({ 0.0f, yaw, 0.0f });
	}
}

void Tongue::UpdateExtending(float deltaTime){
	float move = speed_ * deltaTime;

	worldPosition_.x += shotDirection_.x * move;
	worldPosition_.y += shotDirection_.y * move;
	worldPosition_.z += shotDirection_.z * move;

	currentDistance_ += move;

	if(currentDistance_ >= maxDistance_){
		state_ = State::Returning;
	}
}

void Tongue::UpdateReturning(float deltaTime){
	Vector3 target = GetMouthWorldPosition();

	Vector3 toTarget = {
		target.x - worldPosition_.x,
		target.y - worldPosition_.y,
		target.z - worldPosition_.z
	};

	float distance = Length(toTarget);

	if(distance <= 0.1f){
		Reset();
		return;
	}

	Vector3 dir = Normalize(toTarget);
	float move = returnSpeed_ * deltaTime;

	if(move >= distance){
		worldPosition_ = target;
		Reset();
		return;
	}

	worldPosition_.x += dir.x * move;
	worldPosition_.y += dir.y * move;
	worldPosition_.z += dir.z * move;

	float tongueYaw = std::atan2(dir.x, dir.z);
	object_->SetRotate({ 0.0f, tongueYaw, 0.0f });
}

Vector3 Tongue::GetMouthWorldPosition() const{
	if(!owner_){
		return { 0.0f, 0.0f, 0.0f };
	}

	Vector3 playerPos = owner_->GetPosition();
	float yaw = owner_->GetYaw();

	Vector3 forward = {
		std::sin(yaw),
		0.0f,
		std::cos(yaw)
	};

	Vector3 right = {
		std::cos(yaw),
		0.0f,
		-std::sin(yaw)
	};

	Vector3 result = playerPos;
	result.x += right.x * localOffset_.x + forward.x * localOffset_.z;
	result.y += localOffset_.y;
	result.z += right.z * localOffset_.x + forward.z * localOffset_.z;

	return result;
}