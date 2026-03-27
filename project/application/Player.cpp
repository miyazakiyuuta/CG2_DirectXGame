#include "Player.h"

#include "3d/Camera.h"
#include "3d/Object3dCommon.h"
#include "CameraController.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

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

float Dot3(const Vector3& a, const Vector3& b){
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

float ComputeOBBSupportRadiusAlongNormal(const CollisionUtility::OBB& obb, const Vector3& normal){
	return
		std::abs(Dot3(obb.axis[0], normal)) * obb.halfLength[0] +
		std::abs(Dot3(obb.axis[1], normal)) * obb.halfLength[1] +
		std::abs(Dot3(obb.axis[2], normal)) * obb.halfLength[2];
}

float AbsDot3(const Vector3& a, const Vector3& b){
	return std::abs(Dot3(a, b));
}

Vector3 Cross3(const Vector3& a, const Vector3& b){
	return {
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	};
}

float ClampFloat(float v, float minV, float maxV){
	return std::max(minV, std::min(v, maxV));
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
	prevAimMode_ = false;
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

bool Player::TryShotTongue(const Vector3& direction){
	if(!tongue_){
		return false;
	}

	if(tongue_->IsBusy()){
		return false;
	}

	if(!ConsumeWater(tongueWaterCost_)){
		return false;
	}

	tongue_->Shot(direction);
	return true;
}

void Player::SetYawFromCamera(float cameraYaw){
	if(!object_){
		return;
	}

	object_->SetRotate({ 0.0f, cameraYaw + modelYawOffset_, 0.0f });
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

	Vector3 start = tongue_->GetPrevPosition();
	Vector3 end = tongue_->GetPosition();
	Vector3 delta = end - start;
	float moveLen = Length3(delta);

	CollisionUtility::Sphere baseSphere = tongue_->GetHitSphere();

	// 半径より大きく飛ぶなら分割数を増やす
	int steps = std::max(1, static_cast<int>(std::ceil(moveLen / std::max(0.05f, baseSphere.radius * 0.5f))));

	for(int s = 1; s <= steps; ++s){
		float t = static_cast<float>(s) / static_cast<float>(steps);

		CollisionUtility::Sphere testSphere = baseSphere;
		testSphere.center = start + delta * t;

		for(const auto& block : *blockColliders_){
			auto hit = CollisionUtility::IntersectSphere_OBB_Detailed(testSphere, block);
			if(!hit.hit){
				continue;
			}

			Vector3 hitNormal = hit.normal;

			Vector3 toPlayer = Normalize3(GetPosition() - hit.point);
			if(Dot3(hitNormal, toPlayer) < 0.0f){
				hitNormal = hitNormal * -1.0f;
			}

			SetupClingSurfaceFromHit(block, hit.point, hitNormal);

			Vector3 hookPos = hit.point + clingSurfaceNormal_ * tongueHookSurfaceOffset_;
			tongue_->SetHooked(hookPos);

			CollisionUtility::OBB playerObb = GetPlayerOBB(GetPosition());
			const float kPullSkin = 0.03f;
			float playerRadiusAlongNormal =
				ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_);

			Vector3 clingAnchorPoint =
				clingSurfaceCenter_ +
				clingSurfaceRight_ * clingHitRightOffset_ +
				clingSurfaceUp_ * clingHitUpOffset_;

			tonguePullTarget_ =
				clingAnchorPoint +
				clingSurfaceNormal_ * (playerRadiusAlongNormal + kPullSkin);

			velocity_ = { 0.0f, 0.0f, 0.0f };
			CancelJumpCharge();
			TransitionTo(MovementState::TonguePulling);
			return;
		}
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
		Vector3 snapped = tonguePullTarget_;
		if(hasClingSurface_){
			snapped = ClampPositionToCurrentClingSurface(snapped);
			ResolveCurrentClingPenetration(snapped);
		}

		object_->SetTranslate(snapped);
		velocity_ = { 0.0f, 0.0f, 0.0f };
		isOnGround_ = false;
		tongue_->StartReturn();
		TransitionTo(MovementState::WallClinging);
		return;
	}

	Vector3 dir = Normalize3(toTarget);
	position += dir * tonguePullSpeed_;
	object_->SetTranslate(position);

	float yaw = std::atan2(dir.x, dir.z) + modelYawOffset_;
	object_->SetRotate({ 0.0f, yaw, 0.0f });
}

void Player::Update(){
	if(!object_){
		return;
	}

	float cameraYaw = 0.0f;
	bool isAimMode = false;
	Vector3 cameraForward = { 0.0f, 0.0f, 1.0f };

	if(cameraController_){
		cameraYaw = cameraController_->GetYaw();
		isAimMode = cameraController_->IsAimMode();
		cameraForward = cameraController_->GetForwardDirection();
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

		case MovementState::TonguePulling:
			UpdateTonguePulling();
			break;
	}

	// エイム中はプレイヤーの正面をカメラへ合わせる
	if(isAimMode){
		SetYawFromCamera(cameraYaw);
	}

	if(input_->IsTriggerMouse(0)){
		TryShotTongue(cameraForward);
	}

	if(tongue_){
		tongue_->Update(1.0f / 60.0f);
		CheckTongueBlockHook();
	}
}

void Player::Draw(){
	if(tongue_){
		tongue_->Draw();
	}
	if(object_){
		object_->Update();
		object_->SetColor({ 1.0f, 1.0f, 1.0f, currentAlpha_ });
		object_->Draw();
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

			if(hasClingSurface_){
				wallRightVec_ = clingSurfaceRight_;
			} else{
				wallRightVec_ = { 1.0f, 0.0f, 0.0f };
			}

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
				ImGui::Text("Shot : Right Click Release");
				ImGui::Text("Pull Target : %.3f %.3f %.3f", tonguePullTarget_.x, tonguePullTarget_.y, tonguePullTarget_.z);

				if(ImGui::Button("Shot Tongue")){
					Vector3 debugDirection = { 0.0f, 0.0f, 1.0f };
					if(cameraController_){
						debugDirection = cameraController_->GetForwardDirection();
					}

					TryShotTongue(debugDirection);
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

	if(!hasClingSurface_){
		TransitionTo(MovementState::Jumping);
		return;
	}

	wallClingGauge_ -= wallClingConsumption_;
	if(wallClingGauge_ <= 0.0f){
		wallClingGauge_ = 0.0f;
		hasClingSurface_ = false;
		TransitionTo(MovementState::Jumping);
		return;
	}

	Vector3 moveVec = { 0.0f, 0.0f, 0.0f };

	if(input_->IsPushKey(DIK_W)){
		moveVec += clingSurfaceUp_;
	}
	if(input_->IsPushKey(DIK_S)){
		moveVec -= clingSurfaceUp_;
	}
	if(input_->IsPushKey(DIK_D)){
		moveVec -= wallRightVec_;
	}
	if(input_->IsPushKey(DIK_A)){
		moveVec += wallRightVec_;
	}

	float len = std::sqrt(moveVec.x * moveVec.x + moveVec.y * moveVec.y + moveVec.z * moveVec.z);
	if(len > 0.0001f){
		moveVec = moveVec * (1.0f / len);
		position += moveVec * wallMoveSpeed_;
	}

	// 先に張り付いた面へ向きをそろえる
	float yaw = std::atan2(-clingSurfaceNormal_.x, -clingSurfaceNormal_.z) + modelYawOffset_;
	object_->SetRotate({ 0.0f, yaw, 0.0f });

	// まず張り付き面の範囲に拘束
	position = ClampPositionToCurrentClingSurface(position);

	// 張り付いている元の面へのめり込みを解消
	ResolveCurrentClingPenetration(position);

	// 他のブロックへのめり込みも解消
	ResolveWallClingBlockCollisions(position);

	// 押し戻し後にもう一度面へそろえる
	position = ClampPositionToCurrentClingSurface(position);

	// 面の外へ出たら壁のぼり解除
	if(!IsInsideCurrentClingSurface(position)){
		hasClingSurface_ = false;
		object_->SetTranslate(position);
		TransitionTo(MovementState::Jumping);
		return;
	}

	if(input_->IsTriggerKey(DIK_SPACE)){
		velocity_ = clingSurfaceNormal_ * 0.25f;
		velocity_.y = jumpPowers_[0];
		hasClingSurface_ = false;
		object_->SetTranslate(position);
		TransitionTo(MovementState::Jumping);
		return;
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
		if(tongue_->IsBusy()){
			tongue_->SetAlpha(1.0f);
		} else{
			tongue_->SetAlpha(currentAlpha_);
		}
	}
}

void Player::SetupClingSurfaceFromHit(
	const CollisionUtility::OBB& block,
	const Vector3& hitPoint,
	const Vector3& hitNormal
){
	clingBlockObb_ = block;
	clingSurfaceNormal_ = Normalize3(hitNormal);

	// どの面に当たったかを、法線に最も近いブロック軸で決める
	int normalAxisIndex = 0;
	float bestDot = AbsDot3(block.axis[0], clingSurfaceNormal_);

	for(int i = 1; i < 3; ++i){
		float d = AbsDot3(block.axis[i], clingSurfaceNormal_);
		if(d > bestDot){
			bestDot = d;
			normalAxisIndex = i;
		}
	}

	Vector3 faceNormal = block.axis[normalAxisIndex];
	if(Dot3(faceNormal, clingSurfaceNormal_) < 0.0f){
		faceNormal = faceNormal * -1.0f;
	}
	clingSurfaceNormal_ = faceNormal;

	// 面の中心
	clingSurfaceCenter_ =
		block.center +
		clingSurfaceNormal_ * block.halfLength[normalAxisIndex];

	// 面上の2軸を決める
	int axisA = (normalAxisIndex + 1) % 3;
	int axisB = (normalAxisIndex + 2) % 3;

	// 上方向に近いほうを面の縦方向にする
	Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
	float dotA = AbsDot3(block.axis[axisA], worldUp);
	float dotB = AbsDot3(block.axis[axisB], worldUp);

	int upAxis = axisA;
	int rightAxis = axisB;
	if(dotB > dotA){
		upAxis = axisB;
		rightAxis = axisA;
	}

	clingSurfaceUp_ = block.axis[upAxis];
	if(Dot3(clingSurfaceUp_, worldUp) < 0.0f){
		clingSurfaceUp_ = clingSurfaceUp_ * -1.0f;
	}

	clingSurfaceRight_ = block.axis[rightAxis];

	// 右方向は right × up = normal になる向きへ揃える
	Vector3 cross = Cross3(clingSurfaceRight_, clingSurfaceUp_);
	if(Dot3(cross, clingSurfaceNormal_) < 0.0f){
		clingSurfaceRight_ = clingSurfaceRight_ * -1.0f;
	}

	clingSurfaceHalfWidth_ = block.halfLength[rightAxis];
	clingSurfaceHalfHeight_ = block.halfLength[upAxis];

	// 既存の壁移動ベクトルにも反映
	wallRightVec_ = clingSurfaceRight_;
	tongueHookNormal_ = clingSurfaceNormal_;
	hasClingSurface_ = true;

	// 面の中の「どこに当たったか」を保存
	Vector3 toHit = hitPoint - clingSurfaceCenter_;
	clingHitRightOffset_ = Dot3(toHit, clingSurfaceRight_);
	clingHitUpOffset_ = Dot3(toHit, clingSurfaceUp_);
}

bool Player::IsInsideCurrentClingSurface(const Vector3& position) const{
	if(!hasClingSurface_){
		return false;
	}

	Vector3 toPos = position - clingSurfaceCenter_;
	float rightDist = Dot3(toPos, clingSurfaceRight_);
	float upDist = Dot3(toPos, clingSurfaceUp_);

	return
		std::abs(rightDist) <= (clingSurfaceHalfWidth_ + wallDetachMargin_) &&
		std::abs(upDist) <= (clingSurfaceHalfHeight_ + wallDetachMargin_);
}

Vector3 Player::ClampPositionToCurrentClingSurface(const Vector3& position) const{
	if(!hasClingSurface_){
		return position;
	}

	Vector3 toPos = position - clingSurfaceCenter_;

	float rightDist = Dot3(toPos, clingSurfaceRight_);
	float upDist = Dot3(toPos, clingSurfaceUp_);

	rightDist = ClampFloat(rightDist, -clingSurfaceHalfWidth_, clingSurfaceHalfWidth_);
	upDist = ClampFloat(upDist, -clingSurfaceHalfHeight_, clingSurfaceHalfHeight_);

	CollisionUtility::OBB playerObb = GetPlayerOBB(position);
	float pushOut = ComputeOBBSupportRadiusAlongNormal(playerObb, clingSurfaceNormal_) + wallKeepDistance_;

	return
		clingSurfaceCenter_ +
		clingSurfaceRight_ * rightDist +
		clingSurfaceUp_ * upDist +
		clingSurfaceNormal_ * pushOut;
}

void Player::ResolveCurrentClingPenetration(Vector3& position) const{
	if(!hasClingSurface_){
		return;
	}

	CollisionUtility::OBB playerObb = GetPlayerOBB(position);
	auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, clingBlockObb_);

	if(!hit.hit){
		return;
	}

	Vector3 pushNormal = hit.normal;

	// 壁の外向きにそろえる
	if(Dot3(pushNormal, clingSurfaceNormal_) < 0.0f){
		pushNormal = pushNormal * -1.0f;
	}

	const float kSkin = 0.001f;

	// 外側へ押し出す
	position += pushNormal * (hit.penetration + kSkin);
}

void Player::ResolveWallClingBlockCollisions(Vector3& position) const{
	if(!blockColliders_){
		return;
	}

	// 2〜3回まわして、複数ブロックへの重なりを少し安定させる
	for(int iteration = 0; iteration < 3; ++iteration){
		bool anyHit = false;

		CollisionUtility::OBB playerObb = GetPlayerOBB(position);

		for(const auto& block : *blockColliders_){
			auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(playerObb, block);
			if(!hit.hit || hit.penetration <= 0.0f){
				continue;
			}

			Vector3 pushNormal = hit.normal;

			// プレイヤー -> ブロック の法線なので、プレイヤーを外へ出す向きへ反転
			pushNormal = pushNormal * -1.0f;

			const float kSkin = 0.001f;
			position += pushNormal * (hit.penetration + kSkin);

			playerObb = GetPlayerOBB(position);
			anyHit = true;
		}

		if(!anyHit){
			break;
		}
	}
}