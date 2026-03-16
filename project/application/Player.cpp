#include "Player.h"

#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

Player::~Player() = default;

namespace{
	float LengthXZ(const Vector3& v){
		return std::sqrt(v.x * v.x + v.z * v.z);
	}
}

void Player::Initialize(
	Object3dCommon* object3dCommon,
	Camera* camera,
	const std::string& modelName,
	const Vector3& startPosition
){
	camera_ = camera;
	input_ = Input::GetInstance();

	object_ = std::make_unique<Object3d>();
	object_->Initialize(object3dCommon);
	object_->SetModel(modelName);
	object_->SetCamera(camera_);
	object_->SetTranslate(startPosition);
	object_->SetRotate({ 0.0f, 0.0f, 0.0f });

	tongue_ = std::make_unique<Tongue>();
	tongue_->Initialize(object3dCommon, camera_, this, "Cube.obj");

	velocity_ = { 0.0f, 0.0f, 0.0f };
	lastMove_ = { 0.0f, 0.0f, 0.0f };
	isOnGround_ = false;

	isChargingJump_ = false;
	isJumpChargeCanceled_ = false;
	chargeTimer_ = 0.0f;
	chargeMaxHoldTimer_ = 0.0f;
	moveState_ = MovementState::Root;
	wallClingGauge_ = maxWallClingGauge_;
}

void Player::SetCamera(Camera* camera){
	camera_ = camera;
	if(object_){
		object_->SetCamera(camera_);
	}
}

void Player::SetPosition(const Vector3& position){
	if(object_){
		object_->SetTranslate(position);
	}
}

Vector3 Player::GetPosition() const{
	if(!object_){
		return { 0.0f, 0.0f, 0.0f };
	}
	return object_->GetTranslate();
}

CollisionUtility::AABB Player::GetPlayerAABB(const Vector3& position) const{
	CollisionUtility::AABB box;
	box.min = {
		position.x - colliderHalfSize_.x,
		position.y - colliderHalfSize_.y,
		position.z - colliderHalfSize_.z
	};
	box.max = {
		position.x + colliderHalfSize_.x,
		position.y + colliderHalfSize_.y,
		position.z + colliderHalfSize_.z
	};
	return box;
}

void Player::ResolveHorizontalCollisions(const Vector3& previousPosition){
	if(!object_ || !blockColliders_){
		return;
	}

	Vector3 position = object_->GetTranslate();

	// 床との接触を横判定に混ぜないため、少しだけ上に持ち上げる
	const float kGroundEpsilon = 0.05f;

	if(position.x != previousPosition.x){
		Vector3 testPos = {
			position.x,
			previousPosition.y + kGroundEpsilon,
			previousPosition.z
		};
		CollisionUtility::AABB playerBox = GetPlayerAABB(testPos);

		for(const auto& block : *blockColliders_){
			if(CollisionUtility::IntersectAABB_AABB(playerBox, block)){
				position.x = previousPosition.x;
				break;
			}
		}
	}

	if(position.z != previousPosition.z){
		Vector3 testPos = {
			position.x,
			previousPosition.y + kGroundEpsilon,
			position.z
		};
		CollisionUtility::AABB playerBox = GetPlayerAABB(testPos);

		for(const auto& block : *blockColliders_){
			if(CollisionUtility::IntersectAABB_AABB(playerBox, block)){
				position.z = previousPosition.z;
				break;
			}
		}
	}

	object_->SetTranslate(position);
}

void Player::ResolveVerticalCollisions(const Vector3& previousPosition){
	if(!object_){
		return;
	}

	Vector3 position = object_->GetTranslate();
	isOnGround_ = false;

	if(!blockColliders_){
		object_->SetTranslate(position);
		return;
	}

	for(const auto& block : *blockColliders_){
		CollisionUtility::AABB playerBox = GetPlayerAABB(position);
		if(!CollisionUtility::IntersectAABB_AABB(playerBox, block)){
			continue;
		}

		float prevBottom = previousPosition.y - colliderHalfSize_.y;
		float prevTop = previousPosition.y + colliderHalfSize_.y;
		float nowBottom = position.y - colliderHalfSize_.y;
		float nowTop = position.y + colliderHalfSize_.y;

		// 上から落ちて床に乗る
		if(velocity_.y <= 0.0f && prevBottom >= block.max.y - 0.01f){
			position.y = block.max.y + colliderHalfSize_.y;
			velocity_.y = 0.0f;
			isOnGround_ = true;
			continue;
		}

		// 下から頭をぶつける
		if(velocity_.y >= 0.0f && prevTop <= block.min.y + 0.01f){
			position.y = block.min.y - colliderHalfSize_.y;
			velocity_.y = 0.0f;
			continue;
		}

		// 万一横や角で縦解決に食い込んだら、詳細判定の最小押し戻しを使う
		CollisionUtility::CollisionResult hit =
			CollisionUtility::IntersectAABB_AABB_Detailed(GetPlayerAABB(position), block);

		if(hit.hit){
			position.x -= hit.normal.x * hit.penetration;
			position.y -= hit.normal.y * hit.penetration;
			position.z -= hit.normal.z * hit.penetration;

			if(hit.normal.y > 0.5f){
				velocity_.y = 0.0f;
				isOnGround_ = true;
			}
			if(hit.normal.y < -0.5f && velocity_.y > 0.0f){
				velocity_.y = 0.0f;
			}
		}
	}

	object_->SetTranslate(position);
}

void Player::Update(float cameraYaw){
	if(!object_){
		return;
	}

	switch(moveState_){
		case MovementState::Root:
		case MovementState::Jumping:{
			Vector3 previousPosition = object_->GetTranslate();

			MoveHorizontal(cameraYaw);
			ResolveHorizontalCollisions(previousPosition);

			UpdateJumpCharge();

			Vector3 beforeVertical = object_->GetTranslate();
			ApplyGravity();
			ResolveVerticalCollisions(beforeVertical);

			if(isOnGround_){
				TransitionTo(MovementState::Root);
			} else{
				TransitionTo(MovementState::Jumping);
			}
			break;
		}

		case MovementState::WallClinging:
			UpdateWallClinging(cameraYaw);
			break;
	}

	if(input_->IsTriggerMouse(0) && tongue_){
		tongue_->Shot();
	}

	if(tongue_){
		tongue_->Update(1.0f / 60.0f);
	}

	object_->Update();
}

void Player::Draw(){
	if(object_){
		object_->Draw();
	}
	if(tongue_){
		tongue_->Draw();
	}
}

void Player::MoveHorizontal(float cameraYaw){
	Vector3 position = object_->GetTranslate();
	lastMove_ = { 0.0f, 0.0f, 0.0f };

	Vector3 forward = { std::sin(cameraYaw), 0.0f, std::cos(cameraYaw) };
	Vector3 right = { std::cos(cameraYaw), 0.0f, -std::sin(cameraYaw) };

	if(input_->IsPushKey(DIK_W)){
		lastMove_.x += forward.x;
		lastMove_.z += forward.z;
	}
	if(input_->IsPushKey(DIK_S)){
		lastMove_.x -= forward.x;
		lastMove_.z -= forward.z;
	}
	if(input_->IsPushKey(DIK_D)){
		lastMove_.x += right.x;
		lastMove_.z += right.z;
	}
	if(input_->IsPushKey(DIK_A)){
		lastMove_.x -= right.x;
		lastMove_.z -= right.z;
	}

	if(input_->IsPushKey(DIK_R)){
		position = { 0.0f, resetHeight_, 0.0f };
		velocity_ = { 0.0f, 0.0f, 0.0f };
		isOnGround_ = false;
		CancelJumpCharge();
		object_->SetTranslate(position);
		return;
	}

	float length = LengthXZ(lastMove_);
	if(length > 0.0f){
		lastMove_.x /= length;
		lastMove_.z /= length;

		position.x += lastMove_.x * moveSpeed_;
		position.z += lastMove_.z * moveSpeed_;

		float yaw = std::atan2(lastMove_.x, lastMove_.z) + modelYawOffset_;
		object_->SetRotate({ 0.0f, yaw, 0.0f });
	}

	object_->SetTranslate(position);
}

void Player::UpdateJumpCharge(){
	if(!IsOnGround()){
		isChargingJump_ = false;
		chargeTimer_ = 0.0f;
		chargeMaxHoldTimer_ = 0.0f;
		isJumpChargeCanceled_ = false;
		return;
	}

	if(input_->IsTriggerKey(DIK_SPACE)){
		isChargingJump_ = true;
		isJumpChargeCanceled_ = false;
		chargeTimer_ = 0.0f;
		chargeMaxHoldTimer_ = 0.0f;
	}

	if(!isChargingJump_){
		return;
	}

	if(input_->IsPushKey(DIK_SPACE)){
		chargeTimer_ += 1.0f;

		int currentLevel = GetCurrentChargeLevel();
		int allowedLevel = GetAllowedChargeLevel();

		if(currentLevel >= allowedLevel){
			chargeMaxHoldTimer_ += 1.0f;
			if(chargeMaxHoldTimer_ >= chargeCancelHoldLimit_){
				CancelJumpCharge();
				return;
			}
		} else{
			chargeMaxHoldTimer_ = 0.0f;
		}
	}

	if(!input_->IsPushKey(DIK_SPACE)){
		if(!isJumpChargeCanceled_){
			int chargeLevel = GetCurrentChargeLevel();
			chargeLevel = std::min(chargeLevel, GetAllowedChargeLevel());
			ExecuteChargedJump(chargeLevel);
			TransitionTo(MovementState::Jumping);
		}

		isChargingJump_ = false;
		chargeTimer_ = 0.0f;
		chargeMaxHoldTimer_ = 0.0f;
		isJumpChargeCanceled_ = false;
	}
}

int Player::GetCurrentChargeLevel() const{
	if(chargeTimer_ >= chargeThresholds_[2]) return 3;
	if(chargeTimer_ >= chargeThresholds_[1]) return 2;
	if(chargeTimer_ >= chargeThresholds_[0]) return 1;
	return 0;
}

int Player::GetAllowedChargeLevel() const{
	return std::clamp(chargeStock_, 0, kMaxChargeLevel_);
}

void Player::ExecuteChargedJump(int chargeLevel){
	chargeLevel = std::clamp(chargeLevel, 0, kMaxChargeLevel_);

	velocity_.y = jumpPowers_[chargeLevel];
	isOnGround_ = false;

	if(chargeLevel > 0){
		chargeStock_ -= chargeLevel;
		if(chargeStock_ < 0){
			chargeStock_ = 0;
		}
	}
}

void Player::CancelJumpCharge(){
	isJumpChargeCanceled_ = true;
	isChargingJump_ = false;
	chargeTimer_ = 0.0f;
	chargeMaxHoldTimer_ = 0.0f;
}

void Player::ApplyGravity(){
	Vector3 position = object_->GetTranslate();

	isOnGround_ = false;

	velocity_.y += gravity_;
	position.y += velocity_.y;

	// 非常用の落下リセット
	if(position.y <= -50.0f){
		position = { 0.0f, resetHeight_, 0.0f };
		velocity_ = { 0.0f, 0.0f, 0.0f };
	}

	object_->SetTranslate(position);
}

const char* Player::GetMovementStateName() const{
	switch(moveState_){
		case MovementState::Root:
			return "Root";
		case MovementState::Jumping:
			return "Jumping";
		case MovementState::WallClinging:
			return "WallClinging";
	}
	return "Unknown";
}

void Player::TransitionTo(MovementState nextState){
	if(moveState_ == nextState){
		return;
	}

	if(moveState_ == MovementState::Root && nextState != MovementState::Root){
		CancelJumpCharge();
	}

	moveState_ = nextState;

	switch(moveState_){
		case MovementState::Root:
			velocity_.y = 0.0f;
			isOnGround_ = true;
			break;

		case MovementState::Jumping:
			isOnGround_ = false;
			break;

		case MovementState::WallClinging:{
			velocity_ = { 0.0f, 0.0f, 0.0f };
			wallClingGauge_ = maxWallClingGauge_;

			float yaw = GetYaw();
			wallRightVec_ = { std::cos(yaw), 0.0f, -std::sin(yaw) };
			isOnGround_ = false;
			break;
		}
	}
}

void Player::DrawImGui(){
	if(ImGui::TreeNode("Player")){
		Vector3 position = GetPosition();

		if(ImGui::TreeNode("Position / Velocity")){
			ImGui::Text("World Position");
			ImGui::Separator();
			ImGui::Text("X : %.3f", position.x);
			ImGui::Text("Y : %.3f", position.y);
			ImGui::Text("Z : %.3f", position.z);

			ImGui::Separator();
			ImGui::Text("Velocity");
			ImGui::Text("VX : %.3f", velocity_.x);
			ImGui::Text("VY : %.3f", velocity_.y);
			ImGui::Text("VZ : %.3f", velocity_.z);

			ImGui::Separator();
			ImGui::Text("OnGround : %s", isOnGround_ ? "true" : "false");
			ImGui::Text("Collider Half : %.2f %.2f %.2f", colliderHalfSize_.x, colliderHalfSize_.y, colliderHalfSize_.z);
			ImGui::Text("BlockCollider Count : %d", blockColliders_ ? static_cast<int>(blockColliders_->size()) : 0);
			ImGui::TreePop();
		}

		if(ImGui::TreeNode("State")){
			ImGui::Text("Current State : %s", GetMovementStateName());
			ImGui::Separator();

			if(ImGui::Button("To Root")){
				TransitionTo(MovementState::Root);
			}
			ImGui::SameLine();
			if(ImGui::Button("To Jumping")){
				TransitionTo(MovementState::Jumping);
			}
			ImGui::SameLine();
			if(ImGui::Button("To WallClinging")){
				TransitionTo(MovementState::WallClinging);
			}

			ImGui::Separator();
			ImGui::Text("Wall Gauge");
			ImGui::ProgressBar(
				maxWallClingGauge_ > 0.0f ? (wallClingGauge_ / maxWallClingGauge_) : 0.0f,
				ImVec2(240.0f, 22.0f)
			);
			ImGui::Text("%.1f / %.1f", wallClingGauge_, maxWallClingGauge_);
			ImGui::TreePop();
		}

		if(ImGui::TreeNode("Jump")){
			ImGui::Text("Space : Hold to Charge Jump");
			ImGui::Separator();

			int stock = GetChargeStock();
			int phase = GetCurrentChargePhase();
			float phaseRate = GetCurrentChargePhaseRate();
			int visibleLevel = GetCurrentVisibleChargeLevel();

			ImGui::Text("Charge Stock : %d", stock);
			ImGui::Text("Current Level : %d", visibleLevel);

			int editableStock = stock;
			if(ImGui::SliderInt("Edit Charge Stock", &editableStock, 0, kMaxChargeLevel_)){
				SetChargeStock(editableStock);
				stock = editableStock;
			}

			if(ImGui::Button("+1 Stock")){
				AddChargeStock(1);
			}
			ImGui::SameLine();
			if(ImGui::Button("-1 Stock")){
				AddChargeStock(-1);
			}
			ImGui::SameLine();
			if(ImGui::Button("Max Stock")){
				SetChargeStock(kMaxChargeLevel_);
			}

			ImGui::Separator();

			if(stock <= 0){
				ImGui::Text("Phase : Normal Jump Only");
			} else{
				if(IsChargeAtMaxPhase()){
					ImGui::Text("Phase : MAX (%d / %d)", visibleLevel, stock);
				} else{
					ImGui::Text("Phase : %d / %d", phase + 1, stock);
				}
			}

			ImGui::ProgressBar(phaseRate, ImVec2(240.0f, 22.0f));
			ImGui::Text("Phase Charge : %d%%", static_cast<int>(phaseRate * 100.0f));

			if(IsChargeAtMaxPhase()){
				ImGui::Text("Holding too long will cancel jump");
			}

			ImGui::TreePop();
		}

		if(ImGui::TreeNode("Tongue")){
			if(tongue_){
				Vector3 tonguePos = tongue_->GetPosition();
				ImGui::Text("Position : %.3f %.3f %.3f", tonguePos.x, tonguePos.y, tonguePos.z);

				const char* stateName = "Idle";
				switch(tongue_->GetState()){
					case Tongue::State::Idle:
						stateName = "Idle";
						break;
					case Tongue::State::Extending:
						stateName = "Extending";
						break;
					case Tongue::State::Returning:
						stateName = "Returning";
						break;
				}

				ImGui::Text("State : %s", stateName);
				ImGui::Text("Shot Key : Z");

				if(ImGui::Button("Shot Tongue")){
					tongue_->Shot();
				}
				ImGui::SameLine();
				if(ImGui::Button("Reset Tongue")){
					tongue_->Reset();
				}
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
}

float Player::GetJumpChargeRate() const{
	int allowedLevel = GetAllowedChargeLevel();
	if(allowedLevel <= 0) return 0.0f;

	float maxTime = (allowedLevel == 1) ? chargeThresholds_[0]
		: (allowedLevel == 2) ? chargeThresholds_[1]
		: chargeThresholds_[2];

	float t = chargeTimer_ / maxTime;
	return std::clamp(t, 0.0f, 1.0f);
}

int Player::GetCurrentVisibleChargeLevel() const{
	return std::min(GetCurrentChargeLevel(), GetAllowedChargeLevel());
}

void Player::AddChargeStock(int amount){
	chargeStock_ += amount;
	chargeStock_ = std::clamp(chargeStock_, 0, kMaxChargeLevel_);
}

void Player::SetChargeStock(int stock){
	chargeStock_ = std::clamp(stock, 0, kMaxChargeLevel_);
}

int Player::GetCurrentChargePhase() const{
	int allowedLevel = GetAllowedChargeLevel();
	if(allowedLevel <= 0) return 0;

	if(chargeTimer_ < chargeThresholds_[0]) return 0;
	if(allowedLevel >= 2 && chargeTimer_ < chargeThresholds_[1]) return 1;
	if(allowedLevel >= 3 && chargeTimer_ < chargeThresholds_[2]) return 2;
	return allowedLevel;
}

float Player::GetCurrentChargePhaseRate() const{
	int allowedLevel = GetAllowedChargeLevel();
	if(allowedLevel <= 0) return 0.0f;

	float start = 0.0f;
	float end = chargeThresholds_[0];
	int phase = GetCurrentChargePhase();

	if(phase <= 0){
		start = 0.0f;
		end = chargeThresholds_[0];
	} else if(phase == 1){
		start = chargeThresholds_[0];
		end = chargeThresholds_[1];
	} else if(phase == 2){
		start = chargeThresholds_[1];
		end = chargeThresholds_[2];
	} else{
		return 1.0f;
	}

	float length = end - start;
	if(length <= 0.0f){
		return 1.0f;
	}

	float t = (chargeTimer_ - start) / length;
	return std::clamp(t, 0.0f, 1.0f);
}

bool Player::IsChargeAtMaxPhase() const{
	return GetCurrentVisibleChargeLevel() >= GetAllowedChargeLevel() &&
		GetAllowedChargeLevel() > 0 &&
		GetCurrentChargePhaseRate() >= 1.0f;
}

float Player::GetYaw() const{
	if(!object_){
		return 0.0f;
	}
	return object_->GetRotate().y;
}

void Player::UpdateWallClinging(float cameraYaw){
	(void)cameraYaw;

	Vector3 position = object_->GetTranslate();

	wallClingGauge_ -= wallClingConsumption_;
	if(wallClingGauge_ <= 0.0f){
		wallClingGauge_ = 0.0f;
		TransitionTo(MovementState::Jumping);
		return;
	}

	Vector3 moveVec = { 0.0f, 0.0f, 0.0f };

	if(input_->IsPushKey(DIK_W)) moveVec.y += 1.0f;
	if(input_->IsPushKey(DIK_S)) moveVec.y -= 1.0f;

	if(input_->IsPushKey(DIK_D)){
		moveVec.x += wallRightVec_.x;
		moveVec.z += wallRightVec_.z;
	}
	if(input_->IsPushKey(DIK_A)){
		moveVec.x -= wallRightVec_.x;
		moveVec.z -= wallRightVec_.z;
	}

	float len = std::sqrt(moveVec.x * moveVec.x + moveVec.y * moveVec.y + moveVec.z * moveVec.z);
	if(len > 0.0f){
		position.x += (moveVec.x / len) * wallMoveSpeed_;
		position.y += (moveVec.y / len) * wallMoveSpeed_;
		position.z += (moveVec.z / len) * wallMoveSpeed_;
	}

	if(input_->IsTriggerKey(DIK_SPACE)){
		velocity_.y = jumpPowers_[0];
		TransitionTo(MovementState::Jumping);
	}

	object_->SetTranslate(position);
}