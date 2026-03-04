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

	velocity_ = { 0.0f, 0.0f, 0.0f };
	lastMove_ = { 0.0f, 0.0f, 0.0f };
	isOnGround_ = false;

	isChargingJump_ = false;
	isJumpChargeCanceled_ = false;
	chargeTimer_ = 0.0f;
	chargeMaxHoldTimer_ = 0.0f;
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

void Player::Update(float cameraYaw){
	if(!object_){
		return;
	}

	MoveHorizontal(cameraYaw);
	UpdateJumpCharge();
	ApplyGravity();

	object_->Update();
	UpdateDebugImGui();
}

void Player::Draw(){
	if(object_){
		object_->Draw();
	}
}

void Player::MoveHorizontal(float cameraYaw){
	Vector3 position = object_->GetTranslate();
	lastMove_ = { 0.0f, 0.0f, 0.0f };

	// カメラ基準の前方・右方向
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

		// 移動方向へ回転
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

	// 仮の床判定
	if(position.y <= groundHeight_){
		position.y = groundHeight_;
		velocity_.y = 0.0f;
		isOnGround_ = true;
	}

	object_->SetTranslate(position);
}

void Player::UpdateDebugImGui(){
	ImGui::Begin("Player Position");

	Vector3 position = GetPosition();

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

	ImGui::End();

	ImGui::Begin("Player Jump");

	ImGui::Text("Space : Hold to Charge Jump");
	ImGui::Separator();

	int stock = GetChargeStock();
	int phase = GetCurrentChargePhase();
	float phaseRate = GetCurrentChargePhaseRate();
	int visibleLevel = GetCurrentVisibleChargeLevel();

	ImGui::Text("Charge Stock : %d", stock);
	ImGui::Text("Current Level : %d", visibleLevel);

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

	ImGui::End();
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