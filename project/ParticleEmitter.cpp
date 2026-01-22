#include "ParticleEmitter.h"
#include "ParticleManager.h"

ParticleEmitter::ParticleEmitter(DirectXCommon* dxCommon, SrvManager* srvManager, const Camera* camera) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	camera_ = camera;
}

void ParticleEmitter::Update(float deltaTime) {
	elapsed_ += deltaTime;
	if (elapsed_ >= frequencyTime_) {
		ParticleManager::GetInstance()->Emit(name_, transform_.translate, count_);
		elapsed_ -= frequencyTime_;
	}
}

void ParticleEmitter::Emit() {
	ParticleManager::GetInstance()->Emit(name_, transform_.translate, count_);
}
