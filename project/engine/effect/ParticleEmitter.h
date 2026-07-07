#pragma once
#include "math/Vector3.h"
#include "effect/ParticleConfig.h"
#include <string>
#include <cstdint>

// パーティクル発生装置。生成するだけでグループ作成とManagerへの登録が済み、
// isActiveの間はemitInterval(秒)ごとに自動で発生する(更新はParticleManagerが行う)
class ParticleEmitter {
public:
	ParticleEmitter(const std::string& groupName, const std::string& textureFilePath,
		const ParticleConfig& config, BlendMode blendMode = BlendMode::Alpha);

	~ParticleEmitter();

	// ParticleManagerに登録して更新してもらうためコピー不可
	ParticleEmitter(const ParticleEmitter&) = delete;
	ParticleEmitter& operator=(const ParticleEmitter&) = delete;

	// タイマーを進めて自動発生させる(通常はParticleManagerが毎フレーム呼ぶ)
	void Update(float deltaTime);

	// 現在位置に手動で1回発生させる
	void Emit();

	// 指定位置に1回だけ発生させる(タイマーとは無関係)
	void EmitAt(const Vector3& position, uint32_t count);

	void DrawImGui();

	void SetPosition(const Vector3& position) { position_ = position; }
	void SetConfig(const ParticleConfig& config) { config_ = config; }
	void SetEmitInterval(float seconds) { emitInterval_ = seconds; }
	void SetCount(uint32_t count) { count_ = count; }
	void SetActive(bool active) { isActive_ = active; }

	const Vector3& GetPosition() const { return position_; }
	ParticleConfig& GetConfig() { return config_; }
	const std::string& GetGroupName() const { return groupName_; }
	bool IsActive() const { return isActive_; }

private:
	std::string groupName_;
	ParticleConfig config_;
	Vector3 position_{};
	uint32_t count_ = 10; // 1回の発生数
	float emitInterval_ = 0.1f; // 発生間隔(秒)
	float elapsed_ = 0.0f; // 経過時間用タイマー
	bool isActive_ = true;
};
