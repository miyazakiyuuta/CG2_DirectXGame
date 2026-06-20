#pragma once
#include "math/Vector3.h"
#include "effect/ParticleConfig.h"
#include <vector>

// 焚き火(継続燃焼) + 手動消火(煙演出) オーケストレータ
class FireEffect {
public:
	FireEffect() = default;

	void Initialize();
	void Update(float deltaTime);

	void Ignite(const Vector3& position); // 着火(燃焼開始)
	void SetPositions(const std::vector<Vector3>& positions);
	void Extinguish();                    // 消火(燃焼停止 + 煙バースト)

	bool IsBurning() const { return state_ == State::Burning; }

private:
	enum class State { Idle, Burning };

	void EmitCore(float deltaTime);
	void EmitFlame(float deltaTime);
	void EmitEmbers(float deltaTime);
	void EmitSmokeBurst(int count);

private:
	State   state_ = State::Idle;
	std::vector<Vector3> positions_;

	// グループ名
	const char* kCore_ = "fire_core";
	const char* kFlame_ = "fire_flame";
	const char* kEmber_ = "fire_ember";
	const char* kSmoke_ = "fire_smoke";

	// ベース設定(Initializeで構築)
	ParticleConfig coreCfg_{};
	ParticleConfig flameCfg_{};
	ParticleConfig emberCfg_{};
	ParticleConfig smokeCfg_{};

	// 発生制御(秒間個数 → 時間積算で間引き発生)
	float coreRate_ = 60.0f;
	float flameRate_ = 120.0f;
	float emberRate_ = 30.0f;
	float coreAccum_ = 0.0f;
	float flameAccum_ = 0.0f;
	float emberAccum_ = 0.0f;

	// 形状パラメータ
	float baseRadius_ = 0.28f;   // 焚き火の底面半径
	float flameSpawnHeight_ = 1.0f;
	int   smokeBurstCount_ = 30; // 消火時の煙個数
};