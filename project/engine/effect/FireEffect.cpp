#include "effect/FireEffect.h"
#include "effect/ParticleManager.h"
#include "utility/Random.h"

#include <numbers>
#include <cmath>

namespace {
	constexpr float kPi = std::numbers::pi_v<float>;

	// 円盤内一様サンプリング(底面に均等に撒く)
	Vector3 DiskOffset(float radius) {
		float ang = Random::GetFloat(0.0f, 2.0f * kPi);
		float r = radius * std::sqrt(Random::GetFloat(0.0f, 1.0f));
		return { std::cos(ang) * r, 0.0f, std::sin(ang) * r };
	}

	// 炎用: 底円盤からランダムな高さに撒き、上ほど中心に寄せて三角形にする
	// hMax: 発生させる高さの上限  taper: 上端での半径の絞り(0=点, 1=絞らない)
	Vector3 FlameSpawn(float radius, float hMax, float taper) {
		float h = hMax * std::sqrt(Random::GetFloat(0.0f, 1.0f)); // 下に密度を寄せる
		float t = h / hMax;                                       // 0(底)→1(上)
		float rScale = 1.0f - (1.0f - taper) * t;                 // 上ほど細く
		float ang = Random::GetFloat(0.0f, 2.0f * kPi);
		float r = radius * rScale * std::sqrt(Random::GetFloat(0.0f, 1.0f));
		return { std::cos(ang) * r, h, std::sin(ang) * r };
	}
}

void FireEffect::Initialize() {
	auto* pm = ParticleManager::GetInstance();
	// 3グループとも circle.png を共用(テクスチャは使い回しでOK)
	pm->CreateParticleGroup(kCore_, "resources/circle.png", BlendMode::Add);
	pm->CreateParticleGroup(kFlame_, "resources/circle.png", BlendMode::Add);
	pm->CreateParticleGroup(kEmber_, "resources/circle.png", BlendMode::Add);
	pm->CreateParticleGroup(kSmoke_, "resources/circle.png", BlendMode::Alpha);

	// --- 芯: 底に居座る明るい塊。炎の連続感を作る ---
	coreCfg_.minScale = { 0.7f, 0.7f, 0.7f };
	coreCfg_.maxScale = { 1.1f, 1.1f, 1.1f };
	coreCfg_.minVelocity = { -0.15f, 0.2f, -0.15f };
	coreCfg_.maxVelocity = { 0.15f, 0.6f,  0.15f };
	coreCfg_.gravity = { 0.0f, 0.6f, 0.0f };
	coreCfg_.drag = 1.5f;
	coreCfg_.startScaleMul = 1.0f;
	coreCfg_.endScaleMul = 0.5f;
	coreCfg_.useColorLerp = true;
	coreCfg_.colorEaseIn = true;
	coreCfg_.startColor = { 1.0f, 0.9f, 0.55f, 1.0f }; // 明るい白黄
	coreCfg_.endColor = { 1.0f, 0.4f, 0.05f, 0.0f };
	coreCfg_.swayStrength = 0.15f;  // 芯はあまり揺らさない
	coreCfg_.swayFrequency = 5.0f;

	// --- 炎本体: 高密度・縮みすぎない ---
	flameCfg_.minScale = { 0.5f, 0.5f, 0.5f };
	flameCfg_.maxScale = { 0.85f, 0.85f, 0.85f };
	flameCfg_.minVelocity = { -0.25f, 0.5f, -0.25f };
	flameCfg_.maxVelocity = { 0.25f, 1.1f,  0.25f }; // 縦の散りを抑え束ねる
	flameCfg_.gravity = { 0.0f, 1.0f, 0.0f };
	flameCfg_.drag = 1.2f;
	flameCfg_.startScaleMul = 1.0f;
	flameCfg_.endScaleMul = 0.35f;            // 0.2→0.35: 点になる前に消える
	flameCfg_.useColorLerp = true;
	flameCfg_.colorEaseIn = true;
	flameCfg_.startColor = { 1.0f, 0.88f, 0.45f, 1.0f };
	flameCfg_.endColor = { 0.9f, 0.12f, 0.0f, 0.0f };
	flameCfg_.swayStrength = 0.4f;              // 0.5→0.4: 散りすぎ防止
	flameCfg_.swayFrequency = 7.0f;

	// --- 火の粉: 細かく速く、上にパッと ---
	emberCfg_.minScale = { -0.12f, 0.5f, -0.12f };;
	emberCfg_.maxScale = { 0.12f, 1.1f,  0.12f };
	emberCfg_.minVelocity = { -0.2f, 1.5f, -0.2f };
	emberCfg_.maxVelocity = { 0.2f, 3.0f,  0.2f };
	emberCfg_.gravity = { 0.0f, 0.4f, 0.0f };
	emberCfg_.drag = 0.6f;
	emberCfg_.startScaleMul = 1.0f;
	emberCfg_.endScaleMul = 0.4f;
	emberCfg_.useColorLerp = true;
	emberCfg_.colorEaseIn = true;
	emberCfg_.startColor = { 1.0f, 0.75f, 0.35f, 1.0f };
	emberCfg_.endColor = { 1.0f, 0.35f, 0.0f,  0.0f };
	emberCfg_.swayStrength = 0.25f;
	emberCfg_.swayFrequency = 5.0f;

	// --- 煙: 膨らみながら上昇・フェード(消火時のみ) ---
	smokeCfg_.minScale = { 0.5f, 0.5f, 0.5f };
	smokeCfg_.maxScale = { 0.9f, 0.9f, 0.9f };
	smokeCfg_.minVelocity = { -0.25f, 0.4f, -0.25f };
	smokeCfg_.maxVelocity = { 0.25f, 0.9f,  0.25f };
	smokeCfg_.gravity = { 0.0f, 0.5f, 0.0f };
	smokeCfg_.drag = 0.7f;
	smokeCfg_.startScaleMul = 0.7f;
	smokeCfg_.endScaleMul = 2.2f;             // もくもく膨らむ
	smokeCfg_.useColorLerp = true;
	smokeCfg_.colorEaseIn = true;
	smokeCfg_.startColor = { 0.18f, 0.18f, 0.18f, 0.55f };
	smokeCfg_.endColor = { 0.45f, 0.45f, 0.45f, 0.0f };
	smokeCfg_.swayStrength = 0.35f;
	smokeCfg_.swayFrequency = 3.0f;
}

void FireEffect::Update(float deltaTime) {
	if (state_ == State::Burning) {
		EmitCore(deltaTime);
		EmitFlame(deltaTime);
		EmitEmbers(deltaTime);
	}
	
}

void FireEffect::Ignite(const Vector3& position) {
	positions_ = { position };
	state_ = State::Burning;
}

void FireEffect::SetPositions(const std::vector<Vector3>& positions) {
	positions_ = positions;
	state_ = State::Burning;
}

void FireEffect::Extinguish() {
	if (state_ != State::Burning) { return; }
	EmitSmokeBurst(smokeBurstCount_);
	state_ = State::Idle;
}

void FireEffect::EmitCore(float deltaTime) {
	auto* pm = ParticleManager::GetInstance();
	coreAccum_ += deltaTime;
	const float interval = 1.0f / coreRate_;
	while (coreAccum_ >= interval) {
		coreAccum_ -= interval;
		for (const Vector3& base : positions_) {
			Vector3 pos = base + DiskOffset(baseRadius_ * 0.55f);
			ParticleConfig cfg = coreCfg_;
			cfg.lifeTime = Random::GetFloat(0.4f, 0.65f);
			pm->Emit(kCore_, pos, cfg, 1);
		}
	}
}

void FireEffect::EmitFlame(float deltaTime) {
	auto* pm = ParticleManager::GetInstance();
	flameAccum_ += deltaTime;
	const float interval = 1.0f / flameRate_;
	while (flameAccum_ >= interval) {
		flameAccum_ -= interval;
		for (const Vector3& base : positions_) {       // ▼ 全発生点で撒く
			Vector3 off = DiskOffset(baseRadius_);
			Vector3 pos = base + off;
			ParticleConfig cfg = flameCfg_;
			cfg.lifeTime = Random::GetFloat(0.55f, 0.9f);
			float inward = 1.2f;
			cfg.minVelocity.x = cfg.maxVelocity.x = -off.x * inward;
			cfg.minVelocity.z = cfg.maxVelocity.z = -off.z * inward;
			pm->Emit(kFlame_, pos, cfg, 1);
		}
	}
}

void FireEffect::EmitEmbers(float deltaTime) {
	auto* pm = ParticleManager::GetInstance();
	emberAccum_ += deltaTime;
	const float interval = 1.0f / emberRate_;
	while (emberAccum_ >= interval) {
		emberAccum_ -= interval;
		for (const Vector3& base : positions_) {
			Vector3 pos = base + DiskOffset(baseRadius_ * 0.7f);
			ParticleConfig cfg = emberCfg_;
			cfg.lifeTime = Random::GetFloat(0.8f, 1.4f);
			pm->Emit(kEmber_, pos, cfg, 1);
		}
	}
}

void FireEffect::EmitSmokeBurst(int count) {
	auto* pm = ParticleManager::GetInstance();
	for (const Vector3& base : positions_) {
		for (int i = 0; i < count; ++i) {
			Vector3 pos = base + DiskOffset(baseRadius_);
			pos.y += Random::GetFloat(0.0f, 0.4f);
			ParticleConfig cfg = smokeCfg_;
			cfg.lifeTime = Random::GetFloat(1.6f, 2.6f);
			pm->Emit(kSmoke_, pos, cfg, 1);
		}
	}
}