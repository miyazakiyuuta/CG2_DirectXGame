#include "player/Player.h"

#include "input/ActionInput.h"
#include "math/CatmullRomSpline.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void Player::Initialize(ActionInput* actionInput) {
	actionInput_ = actionInput;

	// 仮モデル(読み込み済みなら早期returnするので重複ロードにはならない)
	ModelManager::GetInstance()->LoadModel("sphere.obj");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("sphere.obj");
	object3d_->SetScale({ 0.5f, 0.5f, 0.5f });
}

void Player::Update(float deltaTime) {
	// 入力→オフセット移動(アクション層経由。DIK_やXInputはここでは見ない)
	offset_.x += actionInput_->GetMoveX() * moveSpeed_ * deltaTime;
	offset_.y += actionInput_->GetMoveY() * moveSpeed_ * deltaTime;
	// 可動範囲はオフセットのクランプで制限する
	offset_.x = std::clamp(offset_.x, -offsetLimit_.x, offsetLimit_.x);
	offset_.y = std::clamp(offset_.y, -offsetLimit_.y, offsetLimit_.y);

	if (rail_ && rail_->GetTotalLength() > 0.0f) {
		// レール座標系(前/右/上)を接線とワールド上方向から作る
		Vector3 forward = rail_->GetTangentByDistance(railDistance_);
		Vector3 right = Vector3::Cross({ 0.0f, 1.0f, 0.0f }, forward);
		if (right.LengthSquared() < 1.0e-6f) {
			// 接線がほぼ真上/真下を向く縮退(外積が潰れる)。暫定でワールドXを右とする
			right = { 1.0f, 0.0f, 0.0f };
		}
		right.Normalize();
		Vector3 up = Vector3::Cross(forward, right); // 直交する2軸の外積なので正規化済み

		// ワールド位置 = レール基準点 + 右×x + 上×y
		worldPosition_ = rail_->GetPositionByDistance(railDistance_) + right * offset_.x + up * offset_.y;

		// 向きはレールカメラと同じ「接線→オイラー角」の逆算(回転規約: 行ベクトル×Rx→Ry→Rz)
		float yaw = std::atan2(forward.x, forward.z);
		float pitch = std::atan2(-forward.y, std::sqrt(forward.x * forward.x + forward.z * forward.z));
		object3d_->SetRotate({ pitch, yaw, 0.0f });
	} else {
		// レール未設定(stage.jsonにrailが無い等)でも入力確認だけはできるようにする
		worldPosition_ = { offset_.x, offset_.y, 0.0f };
	}

	object3d_->SetTranslate(worldPosition_);
	object3d_->Update(deltaTime);
}

void Player::Draw() {
	object3d_->Draw();
}

void Player::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Player");
	ImGui::Text("Offset: (%.2f, %.2f)", offset_.x, offset_.y);
	ImGui::Text("World: (%.1f, %.1f, %.1f)", worldPosition_.x, worldPosition_.y, worldPosition_.z);
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.0f, 50.0f, "%.1f m/s");
	ImGui::DragFloat2("Offset Limit", &offsetLimit_.x, 0.1f, 0.0f, 20.0f);

	// Shoot/Boostは⑨時点では定義のみ(動作なし)。マッピング確認用に押下状態だけ見せる
	ImGui::SeparatorText("Actions (mapping check)");
	ImGui::Text("Shoot: %s", actionInput_->IsPress(ActionInput::Action::Shoot) ? "PRESS" : "-");
	ImGui::Text("Boost: %s", actionInput_->IsPress(ActionInput::Action::Boost) ? "PRESS" : "-");
	ImGui::End();
#endif
}

void Player::SetCamera(Camera* camera) {
	object3d_->SetCamera(camera);
}

Player::Player() = default;

Player::~Player() = default;
