#pragma once

#include "io/Input.h"
#include "math/Vector3.h"

class Camera;

class CameraController{
public:
	void Initialize(Camera* camera);

	// プレイヤー位置を中心に、独立した yaw / pitch で回る
	void Update(const Vector3& target);

	void SetTargetOffset(const Vector3& offset){ targetOffset_ = offset; }
	void SetDistance(float distance){ distance_ = distance; }
	void SetHeight(float height){ height_ = height; }

	void SetYawSpeed(float speed){ yawSpeed_ = speed; }
	void SetPitchSpeed(float speed){ pitchSpeed_ = speed; }

	float GetYaw() const{ return yaw_; }
	float GetPitch() const{ return pitch_; }

private:
	Camera* camera_ = nullptr;
	Input* input_ = nullptr;

	float yaw_ = 0.0f;
	float pitch_ = 0.35f;

	float distance_ = 10.0f;
	float height_ = 2.5f;

	float yawSpeed_ = 0.03f;
	float pitchSpeed_ = 0.02f;

	Vector3 targetOffset_ = { 0.0f, 1.0f, 0.0f };
};