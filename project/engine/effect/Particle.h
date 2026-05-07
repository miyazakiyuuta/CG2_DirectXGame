#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include <cstdint>

struct ParticleCS {
	Vector3 translate;
	Vector3 scale;
	float lifeTime;
	Vector3 velocity;
	float currentTime;
	Vector4 color;
};

struct PerView {
	Matrix4x4 viewProjection;
	Matrix4x4 billboardMatrix;
};

struct EmitterSphere {
	Vector3  translate;      // 位置
	float    radius;         // 射出半径
	uint32_t count;          // 射出数
	float    frequency;      // 射出間隔
	float    frequencyTime;  // 射出間隔調整用時間
	uint32_t emit;           // 射出許可
};

// Particle.h に追加
struct PerFrame {
    float time;
	float deltaTime;
	float padding[2];
};