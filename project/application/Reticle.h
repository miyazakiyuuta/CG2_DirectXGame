#pragma once

#include <memory>
#include <vector>

#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "utility/CollisionUtility.h"
#include "base/WinApp.h"

class Sprite;
class SpriteCommon;
class Camera;
class CameraController;
class Player;

class Reticle{
public:
	void Initialize(
		SpriteCommon* spriteCommon,
		Camera* camera,
		CameraController* cameraController,
		Player* player,
		const std::vector<CollisionUtility::OBB>* blockColliders
	);

	void Update();
	void Draw();

	void SetVisible(bool visible){ isVisible_ = visible; }
	bool IsVisible() const{ return isVisible_; }

	void SetCenter(const Vector2& center){ center_ = center; }
	const Vector2& GetCenter() const{ return center_; }

	void SetColor(const Vector4& color);
	void SetScale(float scale){ scale_ = scale; }
	void SetSpread(float spread){ spread_ = spread; }

	float GetScale() const{ return scale_; }
	float GetSpread() const{ return spread_; }

	bool HasAimTargetPoint() const{ return hasAimTargetPoint_; }
	const Vector3& GetAimTargetPoint() const{ return aimTargetPoint_; }

private:
	void UpdateVisual();
	void ApplyLayout();
	void UpdateAimTarget();

	CollisionUtility::RayHitResult CastCameraAimRay() const;
	CollisionUtility::RayHitResult CastPlayerAimRay(const Vector3& targetPoint) const;

private:
	SpriteCommon* spriteCommon_ = nullptr;
	Camera* camera_ = nullptr;
	CameraController* cameraController_ = nullptr;
	Player* player_ = nullptr;
	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;

	std::unique_ptr<Sprite> centerDot_ = nullptr;
	std::unique_ptr<Sprite> barUp_ = nullptr;
	std::unique_ptr<Sprite> barDown_ = nullptr;
	std::unique_ptr<Sprite> barLeft_ = nullptr;
	std::unique_ptr<Sprite> barRight_ = nullptr;

	Vector2 center_ = { static_cast<float>(WinApp::kClientWidth) / 2.0f, static_cast<float>(WinApp::kClientHeight) / 2.0f};
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 0.9f };
	Vector4 normalColor_ = { 1.0f, 1.0f, 1.0f, 0.9f };
	Vector4 reachableColor_ = { 1.0f, 0.2f, 0.2f, 0.95f };
	bool canReachTongueTarget_ = false;

	bool isVisible_ = true;

	float scale_ = 1.0f;
	float spread_ = 14.0f;

	float centerSize_ = 4.0f;
	float barWidth_ = 12.0f;
	float barHeight_ = 2.0f;
	float barLengthVertical_ = 12.0f;
	float barThicknessVertical_ = 2.0f;

	Vector3 aimTargetPoint_ = { 0.0f, 0.0f, 0.0f };
	bool hasAimTargetPoint_ = false;

	float aimMaxDistance_ = 50.0f;
};