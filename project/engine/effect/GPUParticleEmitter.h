#pragma once
#include "math/Vector3.h"
#include "effect/GPUParticleConfig.h"
#include <string>
#include <cstdint>

// GPUパーティクルの発生装置。生成するだけでグループ作成とManagerへの登録が済み、
// isActiveの間はfrequency(秒)ごとにGPU上のEmit CSがcount個ずつ自動発生させる。
// パラメータのCBV書き込みと射出タイミング判定はGPUParticleManagerが毎フレーム行うので
// シーンからUpdateを呼ぶ必要はない。1グループにつきエミッタは1つを想定。
class GPUParticleEmitter {
public:
	GPUParticleEmitter(const std::string& groupName, const std::string& textureFilePath,
		const GPUParticleConfig& config);

	~GPUParticleEmitter();

	// GPUParticleManagerに登録して参照してもらうためコピー不可
	GPUParticleEmitter(const GPUParticleEmitter&) = delete;
	GPUParticleEmitter& operator=(const GPUParticleEmitter&) = delete;

	void DrawImGui();

	void SetPosition(const Vector3& position) { position_ = position; }
	void SetRadius(float radius) { radius_ = radius; }
	void SetEmitCount(uint32_t count) { emitCount_ = count; }
	void SetFrequency(float seconds) { frequency_ = seconds; }
	void SetActive(bool active) { isActive_ = active; }
	void SetConfig(const GPUParticleConfig& config) { config_ = config; }

	const Vector3& GetPosition() const { return position_; }
	float GetRadius() const { return radius_; }
	uint32_t GetEmitCount() const { return emitCount_; }
	float GetFrequency() const { return frequency_; }
	bool IsActive() const { return isActive_; }
	GPUParticleConfig& GetConfig() { return config_; }
	const std::string& GetGroupName() const { return groupName_; }

private:
	std::string groupName_;
	GPUParticleConfig config_;
	Vector3 position_{};
	float radius_ = 1.0f;       // 射出半径(位置を中心とした±radiusの範囲に発生)
	uint32_t emitCount_ = 100;  // 1回の発生数
	float frequency_ = 0.1f;    // 発生間隔(秒)
	bool isActive_ = true;
};
