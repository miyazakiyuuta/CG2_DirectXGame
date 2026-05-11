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

	static Vector3 RotateY(const Vector3& v, float angleRad){
		float s = std::sin(angleRad);
		float c = std::cos(angleRad);
		return { v.x * c - v.z * s, v.y, v.x * s + v.z * c };
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
	object_->SetScale({ 0.3f, 0.3f, 0.3f });

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

		case State::Hooked:
			prevWorldPosition_ = worldPosition_;
			worldPosition_ = hookPosition_;
			break;

		case State::Returning:
			UpdateReturning(deltaTime);
			break;
	}

	object_->SetTranslate(worldPosition_);
}

void Tongue::Draw(){
	if(object_){
		object_->Update();
		object_->SetColor({ 0.969f, 0.678f, 0.765f, currentAlpha_ });
		object_->SetDissolve(1.0f - currentAlpha_);
		object_->Draw();
	}
}

void Tongue::Shot(const Vector3& direction){
	if(!owner_ || state_ != State::Idle){
		return;
	}

	currentExtendSpeed_ = normalExtendSpeed_;
	currentReturnSpeed_ = normalReturnSpeed_;

	shotDirection_ = Normalize(direction);
	sweeping_ = false;

	Vector3 mouthPos = GetMouthWorldPosition();

	const float kShotStartForwardOffset = 0.15f;
	shotStartPosition_ = mouthPos + shotDirection_ * kShotStartForwardOffset;

	worldPosition_ = shotStartPosition_;
	prevWorldPosition_ = worldPosition_;
	hookPosition_ = {};
	currentDistance_ = 0.0f;

	state_ = State::Extending;

	float tongueYaw = std::atan2(shotDirection_.x, shotDirection_.z);
	object_->SetRotate({ 0.0f, tongueYaw, 0.0f });
}

void Tongue::ShotSweep(const Vector3& direction, float halfAngleDeg){
	if(!owner_ || state_ != State::Idle){
		return;
	}

	baseDirection_ = direction;
	baseDirection_.y = 0.0f;
	baseDirection_ = Normalize(baseDirection_);

	sweepHalfAngleRad_ = halfAngleDeg * (3.14159265f / 180.0f);
	sweepLaunchDirection_ = Normalize(RotateY(baseDirection_, -sweepHalfAngleRad_));

	// duration が正なら引数優先、そうでなければ既定値
	sweepDuration_ = sweepArcDuration_;
	sweepTimer_ = 0.0f;
	sweeping_ = true;

	// 振る攻撃専用速度を使う
	currentExtendSpeed_ = sweepExtendSpeed_;
	currentReturnSpeed_ = sweepReturnSpeed_;

	shotDirection_ = baseDirection_;

	Vector3 mouth = GetMouthWorldPosition();
	shotStartPosition_ = {
		mouth.x + sweepLaunchDirection_.x * localOffset_.z,
		mouth.y + localOffset_.y,
		mouth.z + sweepLaunchDirection_.z * localOffset_.z
	};

	worldPosition_ = shotStartPosition_;
	prevWorldPosition_ = worldPosition_;
	hookPosition_ = {};
	currentDistance_ = 0.0f;

	state_ = State::Extending;

	float tongueYaw = std::atan2(shotDirection_.x, shotDirection_.z);
	object_->SetRotate({ 0.0f, tongueYaw, 0.0f });
}

void Tongue::SetHooked(const Vector3& worldPos){
	prevWorldPosition_ = worldPosition_;
	hookPosition_ = worldPos;
	worldPosition_ = worldPos;
	state_ = State::Hooked;

	if(object_){
		object_->SetTranslate(worldPosition_);
	}
}

void Tongue::StartReturn(){
	if(state_ != State::Idle){
		prevWorldPosition_ = worldPosition_;
		state_ = State::Returning;
	}
}

void Tongue::Reset(){
	state_ = State::Idle;
	currentDistance_ = 0.0f;
	shotDirection_ = { 0.0f, 0.0f, 1.0f };
	shotStartPosition_ = {};
	hookPosition_ = {};
	worldPosition_ = GetMouthWorldPosition();
	prevWorldPosition_ = worldPosition_;

	// 通常ショット速度へ戻す
	currentExtendSpeed_ = normalExtendSpeed_;
	currentReturnSpeed_ = normalReturnSpeed_;

	sweeping_ = false;
	sweepTimer_ = 0.0f;

	if(object_){
		object_->SetTranslate(worldPosition_);
		object_->SetRotate({ 0.0f, 0.0f, 0.0f });
		object_->Update();
	}
}

void Tongue::SetHookPositionPreserveState(const Vector3& worldPos){
	hookPosition_ = worldPos;

	// Hooked 中だけは見た目の位置も追従させる
	if(state_ == State::Hooked){
		prevWorldPosition_ = worldPosition_;
		worldPosition_ = worldPos;

		if(object_){
			object_->SetTranslate(worldPosition_);
		}
	}
}

void Tongue::UpdateIdle(){
	prevWorldPosition_ = worldPosition_;
	worldPosition_ = GetMouthWorldPosition();

	if(owner_){
		float yaw = owner_->GetYaw();

		// 直前に撃った方向が残っているなら、その見た目を少し優先
		if(std::abs(shotDirection_.x) > 0.0001f || std::abs(shotDirection_.z) > 0.0001f){
			yaw = std::atan2(shotDirection_.x, shotDirection_.z);
		}

		object_->SetRotate({ 0.0f, yaw, 0.0f });
	}
}

void Tongue::UpdateExtending(float deltaTime){
    prevWorldPosition_ = worldPosition_;

    // スイープ攻撃中の特別な挙動
	if(sweeping_){
        // 距離がまだmaxDistance_に達していないなら、通常の伸びる挙動で進む
        if(currentDistance_ < maxDistance_){
			float move = currentExtendSpeed_ * deltaTime;
            // スイープ中は、舌先が当たり判定から少し先まで伸びるようにするため、
			// maxDistance_をextraExtendDistance_分だけ先に設定しておく
			float remain = maxDistance_ - currentDistance_;
			float actual = std::min(move, remain);
            currentDistance_ += actual;

			// 舌の位置は、口元からsweepLaunchDirection_に沿ってcurrentDistance_だけ伸びた位置
			Vector3 mouth = GetMouthWorldPosition();
			worldPosition_ = {
				mouth.x + sweepLaunchDirection_.x * currentDistance_,
				mouth.y + sweepLaunchDirection_.y * currentDistance_,
				mouth.z + sweepLaunchDirection_.z * currentDistance_
			};

			// 舌の見た目の向きは、sweepLaunchDirection_に沿うようにする
			float tongueYaw = std::atan2(sweepLaunchDirection_.x, sweepLaunchDirection_.z);
			object_->SetRotate({ 0.0f, tongueYaw, 0.0f });
			
			return;
		}

		// maxDistance_に達したら、舌の位置はsweepHalfAngleRad_の範囲で扇状に動かす
		sweepTimer_ += deltaTime;
		float t = sweepDuration_ > 1e-6f ? (sweepTimer_ / sweepDuration_) : 1.0f;
		t = std::clamp(t, 0.0f, 1.0f);
		float angle = -sweepHalfAngleRad_ + t * (2.0f * sweepHalfAngleRad_);

		Vector3 dir = Normalize(RotateY(baseDirection_, angle));
		Vector3 mouth = GetMouthWorldPosition();
		worldPosition_ = {
			mouth.x + dir.x * maxDistance_,
			mouth.y + dir.y * maxDistance_,
			mouth.z + dir.z * maxDistance_
		};

		// 舌の見た目の向きも、動いている方向に沿うようにする
		float tongueYaw = std::atan2(dir.x, dir.z);
		object_->SetRotate({ 0.0f, tongueYaw, 0.0f });

		if(t >= 1.0f){
			sweeping_ = false;
			state_ = State::Returning;
		}

		return;
	}

	// 通常の伸びる挙動
	float move = currentExtendSpeed_ * deltaTime;
	Vector3 dir = shotDirection_;

	worldPosition_.x += dir.x * move;
	worldPosition_.y += dir.y * move;
	worldPosition_.z += dir.z * move;

	currentDistance_ += move;

	// 最大距離に達したら、フック失敗として戻る
	if(currentDistance_ >= maxDistance_){
		state_ = State::Returning;
	}
}

void Tongue::UpdateReturning(float deltaTime){
	prevWorldPosition_ = worldPosition_;

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
	float move = currentReturnSpeed_ * deltaTime;

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

Vector3 Tongue::GetMouthWorldPosition() const {
	if (!owner_) {
		return { 0.0f, 0.0f, 0.0f };
	}

	// 口元ボーン基準のワールド座標をそのまま使う
	return owner_->GetHeadbornPosition();
}

CollisionUtility::Sphere Tongue::GetHitSphere() const{
	CollisionUtility::Sphere sphere;
	sphere.center = worldPosition_;
	sphere.radius = hitRadius_;
	return sphere;
}