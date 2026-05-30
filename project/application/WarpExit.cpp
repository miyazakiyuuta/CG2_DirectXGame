#define NOMINMAX
#include "WarpExit.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Camera.h"
#include "effect/ParticleManager.h"
#include "effect/ParticleConfig.h"
#include "effect/RingManager.h"
#include "utility/Random.h"
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

	// 出口ポータルの視覚効果（脈動リングと吸い込みパーティクル）
	static float warpPortalTimer = 0.0f;
	warpPortalTimer += 1.0f / 60.0f;
	if (std::fmod(warpPortalTimer, 0.5f) < 0.02f) {
		RingConfig rCfg;
		rCfg.lifeTime = 0.6f;
		rCfg.startScale = exitScale_.x * 0.5f;
		rCfg.endScale = exitScale_.x * 1.2f;
		rCfg.startColor = {0.2f, 0.9f, 0.8f, 0.8f};
		rCfg.endColor = {0.2f, 0.9f, 0.8f, 0.0f};
		rCfg.isBillboard = true;
		RingManager::GetInstance()->Emit(position_, {0,0,0}, rCfg);
	}

	for (int i = 0; i < 2; i++) {
		float angle = Random::GetFloat(0.0f, 6.283185f);
		float radius = exitScale_.x; 
		Vector3 spawnPos = position_ + Vector3{std::cos(angle)*radius, std::sin(angle)*radius, 0.0f};
		
		Vector3 toCenter = position_ - spawnPos;
		float len = std::sqrt(toCenter.x*toCenter.x + toCenter.y*toCenter.y + toCenter.z*toCenter.z);
		float speed = 2.0f;
		if (len > 0.0f) {
			toCenter.x = (toCenter.x / len) * speed;
			toCenter.y = (toCenter.y / len) * speed;
			toCenter.z = (toCenter.z / len) * speed;
		}
		
		Vector3 v1 = {toCenter.x * 0.8f, toCenter.y * 0.8f, toCenter.z * 0.8f};
		Vector3 v2 = {toCenter.x * 1.2f, toCenter.y * 1.2f, toCenter.z * 1.2f};
		ParticleConfig cfg;
		cfg.minVelocity = {(std::min)(v1.x, v2.x), (std::min)(v1.y, v2.y), (std::min)(v1.z, v2.z)};
		cfg.maxVelocity = {(std::max)(v1.x, v2.x), (std::max)(v1.y, v2.y), (std::max)(v1.z, v2.z)};
		cfg.lifeTime = radius / speed;
		cfg.startColor = {0.2f, 0.9f, 1.0f, 1.0f};
		cfg.blendMode = BlendMode::Add;
		cfg.minScale = {0.1f, 0.1f, 0.1f};
		cfg.maxScale = {0.1f, 0.1f, 0.1f};
		
		try {
			ParticleManager::GetInstance()->Emit("warp_suck", spawnPos, cfg, 1);
		} catch(...) {}
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
