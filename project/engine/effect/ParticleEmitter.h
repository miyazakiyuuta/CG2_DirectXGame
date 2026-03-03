#pragma once
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include "math/Vector3.h"
#include "math/Transform.h"
#include "effect/ParticleConfig.h"

class ParticleEmitter {
public:
	ParticleEmitter(DirectXCommon* dxCommon, SrvManager* srvManager, const Camera* camera);

	void Update(float deltaTime);

	void Emit();

public:
	Transform transform_{}; // 発生場所
	std::string name_;
	uint32_t count_ = 1; // 発生数
	float elapsed_ = 0.0f; // 経過時間用タイマー
	float frequencyTime_ = 30.0f; // 発生頻度 
	bool isActive_ = true;
	ParticleConfig config_; // 設定

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;
	
};

