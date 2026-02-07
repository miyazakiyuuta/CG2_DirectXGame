#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"
#include "engine/base/Vector3.h"
#include "Transform.h"

class ParticleEmitter {
public:
	ParticleEmitter(DirectXCommon* dxCommon, SrvManager* srvManager, const Camera* camera);

	void Update(float deltaTime);

	void Emit();

public:
	Transform transform_{};
	std::string name_;
	uint32_t count_ = 1;
	float elapsed_ = 0.0f;
	float frequencyTime_ = 30.0f;
	bool isActive_ = true;

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;
};

