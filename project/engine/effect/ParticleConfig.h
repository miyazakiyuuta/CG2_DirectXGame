#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"

enum class ParticleMoveType {
	Normal,
};

enum class BlendMode {
	Alpha,
	Add,
	None,
	Multiply,
};

struct ParticleConfig {
	ParticleMoveType moveType = ParticleMoveType::Normal;
	Vector3 minScale = { 1.0f,1.0f,1.0f };
	Vector3 maxScale = { 1.0f,1.0f,1.0f };
	Vector3 minRotate = { 0.0f,0.0f,0.0f };
	Vector3 maxRotate = { 0.0f,0.0f,0.0f };
	Vector3 minVelocity = { -1.0f,-1.0f,-1.0f };
	Vector3 maxVelocity = { 1.0f,1.0f,1.0f };
	float lifeTime = 1.0f; // 寿命
	Vector4 startColor = { 1.0,1.0f,1.0f,1.0f };
	BlendMode blendMode = BlendMode::Alpha;
	
	bool    useRadialVelocity = false;          // trueで球状の放射初速
	float   minSpeed = 1.0f;
	float   maxSpeed = 1.0f;
	Vector3 gravity = { 0.0f, 0.0f, 0.0f };      // m/s^2
	float   drag = 0.0f;                         // 0=減速なし。大きいほど早く止まる(per sec)
	float   startScaleMul = 1.0f;                // 生成時のスケール倍率
	float   endScaleMul = 1.0f;                  // 死亡時のスケール倍率
	bool    useColorLerp = false;                // trueでstart→endColorを補間
	Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };

	bool colorEaseIn = false;

	float swayStrength = 0.0f;
	float swayFrequency = 6.0f;
};