#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"

enum class ParticleMoveType {
	Normal,
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
	// 重力などの設定要素を増やしたければ以下に追加
};