#define NOMINMAX
#include "WarpExit.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Camera.h"
#include <cmath>

void WarpExit::Initialize(Object3dCommon* objCommon, Camera* camera,
                           const Vector3& position, const std::string& modelName)
{
	position_ = position;

	// 3D object for rendering
	object_ = std::make_unique<Object3d>();
	object_->Initialize(objCommon);
	object_->SetModel(modelName);
	object_->SetCamera(camera);
	object_->SetTranslate(position_);
	object_->SetScale(exitScale_);
	object_->SetColor(exitColor_);
}

void WarpExit::Update(const Vector3& playerPosition)
{
	// --- Lifetime countdown ---
	if (lifetimeFrames_ > 0) {
		--lifetimeFrames_;
	}

	// --- Player overlap detection (sphere) ---
	// Calculate distance between player and exit center
	float dx = playerPosition.x - position_.x;
	float dy = playerPosition.y - position_.y;
	float dz = playerPosition.z - position_.z;
	float distSq = dx * dx + dy * dy + dz * dz;
	float radiusSq = triggerRadius_ * triggerRadius_;

	bool wasInside = playerCurrentlyInside_;
	playerCurrentlyInside_ = (distSq <= radiusSq);

	// Begin Overlap: player just entered
	if (playerCurrentlyInside_ && !wasInside) {
		hasPlayerEntered_ = true;
	}

	// End Overlap: player was inside and just left
	if (!playerCurrentlyInside_ && wasInside && hasPlayerEntered_) {
		playerLeftAfterEnter_ = true;
	}

	// Update 3D object
	if (object_) {
		object_->Update();
	}
}

void WarpExit::Draw()
{
	if (object_) {
		object_->Draw();
	}
}

bool WarpExit::IsExpired() const
{
	// Condition 1: lifetime has run out
	if (lifetimeFrames_ <= 0) {
		return true;
	}

	// Condition 2: player entered then left the trigger volume
	if (playerLeftAfterEnter_) {
		return true;
	}

	return false;
}
