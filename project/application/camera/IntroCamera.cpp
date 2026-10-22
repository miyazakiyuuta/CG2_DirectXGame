#include "camera/IntroCamera.h"

#include "math/Easing.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 仰角の上限[度]。真上(90度)から見下ろすと「自機を見る向き」が水平成分を持たず、
	// yaw(atan2(x, z))が決まらなくなる(画面が急に回る)ので、手前で止める
	constexpr float kMaxElevationDegree = 80.0f;
	constexpr float kMaxElevation = kMaxElevationDegree * std::numbers::pi_v<float> / 180.0f;
}

void IntroCamera::Start() {
	timer_ = 0.0f;
	playing_ = true;
}

void IntroCamera::Stop() {
	// 経過時間も終点へ進めておく(ImGuiの進み具合が止めた後も1.0を指すように)
	timer_ = params_.holdDuration + params_.moveDuration;
	playing_ = false;
}

void IntroCamera::Update(float deltaTime) {
	if (!playing_) {
		return;
	}
	timer_ += deltaTime;
	if (timer_ >= params_.holdDuration + params_.moveDuration) {
		playing_ = false; // 定位置(=レールカメラの位置)に着いた
	}
}

float IntroCamera::GetProgress() const {
	if (params_.moveDuration <= 0.0f) {
		return 1.0f;
	}
	return std::clamp((timer_ - params_.holdDuration) / params_.moveDuration, 0.0f, 1.0f);
}

void IntroCamera::ComputePose(const Vector3& target, const Vector3& forward, float backDistance,
	Vector3& outTranslate, Vector3& outRotate) const {
	// 方位・仰角・距離を同じカーブで終点へ寄せる。InOutCubicは動き出しと止まり際が緩やかなので、
	// 溜めから自然に動き出し、レールカメラの定位置へ速さ0で着く(着いた瞬間にカクッとしない)
	const float t = Easing::InOutCubic(GetProgress());
	const float azimuth = std::lerp(params_.startAzimuth, 0.0f, t);
	const float elevation = std::lerp((std::min)(params_.startElevation, kMaxElevation), 0.0f, t);
	const float distance = std::lerp(params_.startDistance, backDistance, t);

	// レールの前/右/上(Player::ComputeRailFrameと同じ作り方)。ワールドの軸ではなくレールの軸で回すので、
	// ステージのレールの始点の向きを変えても、イントロはその向きに合わせて回り込む
	Vector3 right = Vector3::Cross({ 0.0f, 1.0f, 0.0f }, forward);
	if (right.LengthSquared() < 1.0e-6f) {
		// 接線がほぼ真上/真下を向く縮退(外積が潰れる)。Playerと同じくワールドXを右とする
		right = { 1.0f, 0.0f, 0.0f };
	}
	right.Normalize();
	const Vector3 up = Vector3::Cross(forward, right);

	// 球の上の位置。方位0・仰角0で -forward(真後ろ)になり、レールカメラと同じ位置になる
	const Vector3 horizontal = -forward * std::cos(azimuth) + right * std::sin(azimuth);
	const Vector3 direction = horizontal * std::cos(elevation) + up * std::sin(elevation);
	outTranslate = target + direction * distance;

	// 自機を見る向き→オイラー角。レールカメラの「接線→オイラー角」と同じ逆算式
	// (回転規約: 行ベクトル×Rx→Ry→Rz で forward = (cos(pitch)·sin(yaw), -sin(pitch), cos(pitch)·cos(yaw)))。
	// 終点では見る向きが接線と一致するので、回転もレールカメラと同じ値になる
	const Vector3 look = target - outTranslate;
	const float yaw = std::atan2(look.x, look.z);
	const float pitch = std::atan2(-look.y, std::sqrt(look.x * look.x + look.z * look.z));
	outRotate = { pitch, yaw, 0.0f }; // バンク(roll)はしない(レールカメラと同じ)
}

void IntroCamera::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Intro Camera");
	ImGui::Text("State: %s", playing_ ? "PLAYING" : "-");
	ImGui::ProgressBar(GetProgress());

	ImGui::SeparatorText("Timing");
	ImGui::DragFloat("Hold", &params_.holdDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Move", &params_.moveDuration, 0.01f, 0.1f, 10.0f, "%.2f s");

	// 開始位置。方位は0=真後ろ / 180=真正面、正で右側から回り込む
	ImGui::SeparatorText("Start Pose");
	ImGui::SliderAngle("Azimuth", &params_.startAzimuth, -180.0f, 180.0f);
	ImGui::SliderAngle("Elevation", &params_.startElevation, 0.0f, kMaxElevationDegree);
	ImGui::DragFloat("Distance", &params_.startDistance, 0.1f, 1.0f, 80.0f, "%.1f m");
	ImGui::End();
#endif
}
