#include "Player.h"

#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "utility/Logger.h"
#include <string>

Player::~Player() = default;

namespace{
	float LengthXZ(const Vector3& v){
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	float Length3(const Vector3& v){
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	Vector3 Normalize3(const Vector3& v){
		float len = Length3(v);
		if(len <= 0.0001f){
			return { 0.0f, 0.0f, 1.0f };
		}
		return { v.x / len, v.y / len, v.z / len };
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

	waterGauge_ = maxWaterGauge_;

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

void Player::SetPendingTeleport(const Vector3& position){
	pendingTeleport_ = true;
	pendingTeleportPosition_ = position;
}

void Player::SetRidingPlatformDelta(const Vector3& delta){
	ridingPlatformDelta_ = delta;
}

void Player::ClearRidingPlatformDelta(){
	ridingPlatformDelta_ = {0.0f,0.0f,0.0f};
}

Vector3 Player::GetPosition() const{
	if(!object_){
		return { 0.0f, 0.0f, 0.0f };
	}
	return object_->GetTranslate();
}

CollisionUtility::OBB Player::GetPlayerOBB(const Vector3& position) const{
	Transform t;
	t.translate = position;
	t.rotate = object_ ? object_->GetRotate() : Vector3{ 0.0f, 0.0f, 0.0f };
	t.scale = { 1.0f, 1.0f, 1.0f };

	return CollisionUtility::MakeOBBFromTransform(t, colliderHalfSize_);
}

void Player::AddWater(float amount){
	waterGauge_ += amount;
	if(waterGauge_ > maxWaterGauge_){
		waterGauge_ = maxWaterGauge_;
	}
	if(waterGauge_ < 0.0f){
		waterGauge_ = 0.0f;
	}
}

bool Player::ConsumeWater(float amount){
	if(waterGauge_ < amount){
		return false;
	}
	waterGauge_ -= amount;
	if(waterGauge_ < 0.0f){
		waterGauge_ = 0.0f;
	}
	return true;
}

void Player::ResolveHorizontalCollisions(const Vector3& previousPosition){
	if(!object_ || !blockColliders_){
		return;
	}

	Vector3 position = object_->GetTranslate();
	const float kGroundEpsilon = 0.05f;

	if(position.x != previousPosition.x){
		Vector3 testPos = {
			position.x,
			previousPosition.y + kGroundEpsilon,
			previousPosition.z
		};

		CollisionUtility::OBB playerObb = GetPlayerOBB(testPos);

		for(const auto& block : *blockColliders_){
			if(CollisionUtility::IntersectOBB_OBB(playerObb, block)){
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

		CollisionUtility::OBB playerObb = GetPlayerOBB(testPos);

		for(const auto& block : *blockColliders_){
			if(CollisionUtility::IntersectOBB_OBB(playerObb, block)){
				position.z = previousPosition.z;
				break;
			}
		}
	}

	object_->SetTranslate(position);
}

void Player::ResolveVerticalCollisions(const Vector3& previousPosition){
	(void)previousPosition;

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
		CollisionUtility::OBB playerObb = GetPlayerOBB(position);
		auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);

		if(!hit.hit){
			continue;
		}

		const float kSkinWidth = 0.001f;
		position -= hit.normal * (hit.penetration + kSkinWidth);

		if(hit.normal.y < -0.5f){
			velocity_.y = 0.0f;
			isOnGround_ = true;
		}
		if(hit.normal.y > 0.5f && velocity_.y > 0.0f){
			velocity_.y = 0.0f;
		}
	}

	object_->SetTranslate(position);
}

void Player::CheckTongueBlockHook(){
	if(!tongue_ || !blockColliders_){
		return;
	}

	if(tongue_->GetState() != Tongue::State::Extending){
		return;
	}

	CollisionUtility::Sphere tongueSphere = tongue_->GetHitSphere();

	for(const auto& block : *blockColliders_){
		auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(tongueSphere, block);
		if(!hit.hit){
			continue;
		}

		Vector3 hookPos = hit.point + hit.normal * tongueHookSurfaceOffset_;
		tongue_->SetHooked(hookPos);
		tonguePullTarget_ = hookPos;

		velocity_ = { 0.0f, 0.0f, 0.0f };
		CancelJumpCharge();
		TransitionTo(MovementState::TonguePulling);
		return;
	}
}

void Player::UpdateTonguePulling(){
	if(!object_ || !tongue_){
		return;
	}

	Vector3 position = object_->GetTranslate();
	Vector3 toTarget = tonguePullTarget_ - position;
	float distance = Length3(toTarget);

	if(distance <= tonguePullEndDistance_){
		object_->SetTranslate(tonguePullTarget_);
		velocity_ = { 0.0f, 0.0f, 0.0f };
		isOnGround_ = false;
		tongue_->StartReturn();
		TransitionTo(MovementState::Jumping);
		return;
	}

	Vector3 dir = Normalize3(toTarget);
	position += dir * tonguePullSpeed_;
	object_->SetTranslate(position);

	float yaw = std::atan2(dir.x, dir.z) + modelYawOffset_;
	object_->SetRotate({ 0.0f, yaw, 0.0f });
}

void Player::Update(float cameraYaw){
	if(!object_){
		return;
	}

	// If an external system requested a teleport, apply it here at the
	// start of the update so player internal logic (collisions, gravity,
	// etc.) won't overwrite the requested position.
	if(pendingTeleport_){
		object_->SetTranslate(pendingTeleportPosition_);
		velocity_ = { 0.0f, 0.0f, 0.0f };
		// reset movement state to jumping so physics will be applied
		// consistently from the new position.
		TransitionTo(MovementState::Jumping);
		pendingTeleport_ = false;
	}

    // Note: ridingPlatformDelta_ will be applied after physics so it is not
	// overwritten by collision resolution. GamePlayScene sets this value each frame
	// when the player is detected to be standing on a moving platform.

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

		case MovementState::TonguePulling:
			UpdateTonguePulling();
			break;
	}

    // Apply riding platform delta after physics and collision resolution so it is
	// not overwritten by movement/collision corrections.
	if(ridingPlatformDelta_.x != 0.0f || ridingPlatformDelta_.y != 0.0f || ridingPlatformDelta_.z != 0.0f){
		Vector3 pos = object_->GetTranslate();
		pos.x += ridingPlatformDelta_.x;
		pos.y += ridingPlatformDelta_.y;
		pos.z += ridingPlatformDelta_.z;
		object_->SetTranslate(pos);

		// Convert per-frame displacement to approximate velocity contribution.
		const float kInvDt = 60.0f;
		velocity_.x += ridingPlatformDelta_.x * kInvDt;
		velocity_.z += ridingPlatformDelta_.z * kInvDt;
		if(std::fabs(ridingPlatformDelta_.y) > 1e-6f) velocity_.y = 0.0f;

		Logger::Log(std::string("Player applied riding delta posAfter:") +
					std::to_string(pos.x) + "," + std::to_string(pos.y) + "," + std::to_string(pos.z) +
					" delta:" + std::to_string(ridingPlatformDelta_.x) + "," + std::to_string(ridingPlatformDelta_.y) + "," + std::to_string(ridingPlatformDelta_.z) + "\n");

		// Clear after applying; GamePlayScene will set it again next frame if still riding.
		ridingPlatformDelta_ = {0.0f,0.0f,0.0f};
	}

	if(input_->IsTriggerMouse(0) && tongue_){
		if(!tongue_->IsBusy() && ConsumeWater(tongueWaterCost_)){
			tongue_->Shot();
		}
	}

	if(tongue_){
		tongue_->Update(1.0f / 60.0f);
		CheckTongueBlockHook();
	}
}

void Player::Draw(){
	if(object_){
		object_->Update();
		object_->SetColor({ 1.0f, 1.0f, 1.0f, currentAlpha_ });
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
		if(tongue_){
			tongue_->Reset();
		}
		TransitionTo(MovementState::Jumping);
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
		if(tongue_){
			tongue_->Reset();
		}
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
		case MovementState::TonguePulling:
			return "TonguePulling";
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

		case MovementState::TonguePulling:
			velocity_ = { 0.0f, 0.0f, 0.0f };
			isOnGround_ = false;
			break;
	}
}

void Player::DrawImGui(){
#ifdef USE_IMGUI
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

		if(ImGui::TreeNode("Water")){
			ImGui::Text("Water Gauge : %.1f / %.1f", waterGauge_, maxWaterGauge_);
			ImGui::ProgressBar(
				maxWaterGauge_ > 0.0f ? (waterGauge_ / maxWaterGauge_) : 0.0f,
				ImVec2(240.0f, 22.0f)
			);
			ImGui::Text("Tongue Cost : %.1f", tongueWaterCost_);
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
			ImGui::SameLine();
			if(ImGui::Button("To TonguePulling")){
				TransitionTo(MovementState::TonguePulling);
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
					case Tongue::State::Hooked:
						stateName = "Hooked";
						break;
					case Tongue::State::Returning:
						stateName = "Returning";
						break;
				}

				ImGui::Text("State : %s", stateName);
				ImGui::Text("Shot Mouse : Left Click");
				ImGui::Text("Pull Target : %.3f %.3f %.3f", tonguePullTarget_.x, tonguePullTarget_.y, tonguePullTarget_.z);

				if(ImGui::Button("Shot Tongue")){
					if(!tongue_->IsBusy() && ConsumeWater(tongueWaterCost_)){
						tongue_->Shot();
					}
				}
				ImGui::SameLine();
				if(ImGui::Button("Reset Tongue")){
					tongue_->Reset();
					if(moveState_ == MovementState::TonguePulling){
						TransitionTo(MovementState::Jumping);
					}
				}
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
#endif
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

void Player::UpdateTransparencyByCamera(const Vector3& cameraPosition){
	if(!object_){
		return;
	}

	Vector3 toPlayer = {
		GetPosition().x - cameraPosition.x,
		GetPosition().y - cameraPosition.y,
		GetPosition().z - cameraPosition.z
	};

	float distance = toPlayer.Length();

	if(distance >= fadeStartDistance_){
		currentAlpha_ = 1.0f;
	} else if(distance <= fadeEndDistance_){
		currentAlpha_ = minAlpha_;
	} else{
		float t = (distance - fadeEndDistance_) / (fadeStartDistance_ - fadeEndDistance_);
		currentAlpha_ = minAlpha_ + (1.0f - minAlpha_) * t;
	}

	if(tongue_){
		tongue_->SetAlpha(currentAlpha_);
	}
}