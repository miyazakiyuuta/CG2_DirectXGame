#include "3d/DebugCamera.h"

#include "base/DirectXCommon.h"
#include "io/Input.h"
#include "math/Matrix4x4.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 真上・真下の手前で止める(視線と上方向が平行になってビュー行列が破綻するのを防ぐ)
	constexpr float kPitchLimit = std::numbers::pi_v<float> / 2.0f - 0.02f;
	// ホイール1ノッチあたりの回転量(DirectInputのlZ)
	constexpr float kWheelNotch = 120.0f;
	// 速度調整の範囲
	constexpr float kMinMoveSpeed = 0.1f;
	constexpr float kMaxMoveSpeed = 100.0f;
	// パンの移動量係数(1ピクセルあたり moveSpeed_ の何倍動かすか)
	constexpr float kPanFactor = 0.005f;
}

void DebugCamera::Initialize(Camera* gameCamera) {
	gameCamera_ = gameCamera;
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
}

void DebugCamera::Update(float deltaTime) {
	if (!isActive_) { return; }

	Input* input = Input::GetInstance();

#ifdef USE_IMGUI
	// SceneビューはImGuiウィンドウ内に描画されるため、ビュー上にマウスがある時だけ操作を受ける
	bool sceneHovered = input->IsSceneViewHovered();
#else
	bool sceneHovered = true;
#endif

	// ドラッグの開始はSceneビュー上のみ(ImGuiウィンドウ操作で視点が動くのを防ぐ)。
	// ドラッグ継続中はカーソルがビュー外へ出ても操作を維持する
	if (!input->IsPressMouse(1)) {
		isFlying_ = false;
	} else if (!isFlying_ && sceneHovered) {
		isFlying_ = true;
	}
	if (!input->IsPressMouse(2)) {
		isPanning_ = false;
	} else if (!isPanning_ && sceneHovered) {
		isPanning_ = true;
	}

	float wheelNotches = input->GetMouseWheel() / kWheelNotch;

	if (isFlying_) {
		// 視点回転
		Input::MouseMove mouse = input->GetMouseMove();
		yaw_ += mouse.x * rotateSensitivity_;
		pitch_ = std::clamp(pitch_ + mouse.y * rotateSensitivity_, -kPitchLimit, kPitchLimit);

		// ドラッグ中のホイールは基本速度の増減
		if (wheelNotches != 0.0f) {
			moveSpeed_ = std::clamp(moveSpeed_ * std::pow(1.2f, wheelNotches), kMinMoveSpeed, kMaxMoveSpeed);
		}

		// WASD+QE でカメラローカル移動
		Vector3 local{};
		if (input->IsPushKey(DIK_W)) { local.z += 1.0f; }
		if (input->IsPushKey(DIK_S)) { local.z -= 1.0f; }
		if (input->IsPushKey(DIK_D)) { local.x += 1.0f; }
		if (input->IsPushKey(DIK_A)) { local.x -= 1.0f; }
		if (input->IsPushKey(DIK_E)) { local.y += 1.0f; }
		if (input->IsPushKey(DIK_Q)) { local.y -= 1.0f; }
		if (local.LengthSquared() > 0.0f) {
			float speed = moveSpeed_;
			if (input->IsPushKey(DIK_LSHIFT) || input->IsPushKey(DIK_RSHIFT)) {
				speed *= shiftMultiplier_;
			}
			Matrix4x4 rotate = Matrix4x4::Rotate(Vector3{ pitch_, yaw_, 0.0f });
			position_ += rotate.Transform(local.Normalize()) * (speed * deltaTime);
		}
	} else if (isPanning_) {
		// 中ドラッグでパン。掴んだ世界をドラッグ方向へ動かす操作感(カメラは逆方向へ移動)
		Input::MouseMove mouse = input->GetMouseMove();
		Vector3 local = {
			-mouse.x * moveSpeed_ * kPanFactor,
			 mouse.y * moveSpeed_ * kPanFactor,
			 0.0f,
		};
		Matrix4x4 rotate = Matrix4x4::Rotate(Vector3{ pitch_, yaw_, 0.0f });
		position_ += rotate.Transform(local);
	} else if (sceneHovered && wheelNotches != 0.0f) {
		// 非ドラッグ時のホイールは前後ドリー(移動速度に比例させて遠近どちらでも使いやすく)
		Matrix4x4 rotate = Matrix4x4::Rotate(Vector3{ pitch_, yaw_, 0.0f });
		position_ += rotate.Transform({ 0.0f, 0.0f, 1.0f }) * (wheelNotches * moveSpeed_ * 0.25f);
	}

	// 内部状態を描画用カメラへ反映
	camera_->SetRotate({ pitch_, yaw_, 0.0f });
	camera_->SetTranslate(position_);
	camera_->Update();
	camera_->TransferToGPU();
}

void DebugCamera::SetActive(bool active) {
	// 初回ONのみゲームカメラの視点から開始する(視点がいきなり飛ぶのを防ぐ)。
	// 2回目以降は前回のデバッグ視点を維持する(作業中の視点を失わない)
	if (active && !isActive_ && !hasActivatedOnce_ && gameCamera_) {
		SyncPoseFrom(*gameCamera_);
	}
	if (active) {
		hasActivatedOnce_ = true;
	}
	isActive_ = active;
	if (!active) {
		isFlying_ = false;
		isPanning_ = false;
	}
}

void DebugCamera::FocusOn(const Vector3& target, float radius) {
	// 現在の視線方向を保ったまま、対象が画面に収まる距離まで寄る
	float distance = (std::max)(radius * 3.0f, 2.0f);
	Vector3 forward = Matrix4x4::Rotate(Vector3{ pitch_, yaw_, 0.0f }).Transform({ 0.0f, 0.0f, 1.0f });
	position_ = target - forward * distance;
}

void DebugCamera::CopyPoseToGameCamera() {
	if (!gameCamera_) { return; }
	gameCamera_->SetRotate({ pitch_, yaw_, 0.0f });
	gameCamera_->SetTranslate(position_);
}

void DebugCamera::SyncPoseFrom(const Camera& src) {
	position_ = src.GetTranslate();
	pitch_ = std::clamp(src.GetRotate().x, -kPitchLimit, kPitchLimit);
	yaw_ = src.GetRotate().y;
	camera_->SetFovY(src.GetFovY());
}

void DebugCamera::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Debug Camera");

	bool active = isActive_;
	if (ImGui::Checkbox("Enable (F1)", &active)) {
		SetActive(active);
	}
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, kMinMoveSpeed, kMaxMoveSpeed);
	ImGui::DragFloat("Sensitivity", &rotateSensitivity_, 0.0001f, 0.0005f, 0.02f, "%.4f");

	if (isActive_) {
		if (ImGui::Button("Copy Pose -> Game Camera")) {
			CopyPoseToGameCamera();
		}
		// デバッグ視点をゲームカメラの位置へ戻す(ONのたびの自動同期はしない)
		if (ImGui::Button("Sync Pose <- Game Camera") && gameCamera_) {
			SyncPoseFrom(*gameCamera_);
		}
		ImGui::TextDisabled("RMB: Look  RMB+WASD/QE: Move  MMB: Pan\nShift: Fast  Wheel: Speed/Dolly  F: Focus");
	}

	ImGui::End();
#endif
}
