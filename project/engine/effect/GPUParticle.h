#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include "effect/GPUParticleConfig.h"
#include <cstdint>

// GPUパーティクル用のGPU転送構造体。GPUParticle.hlsli側の同名構造体と必ず一致させること

// 粒子1粒(StructuredBufferなのでパディング不要。全て0=死亡状態)
struct ParticleCS {
	Vector3 translate;
	Vector3 scale;
	Vector3 baseScale; // 生成時スケール(寿命補間の基準)
	float lifeTime;
	Vector3 velocity;
	float currentTime;
	Vector4 color;
};

struct PerView {
	Matrix4x4 viewProjection;
	Matrix4x4 billboardMatrix;
};

// 継続放出エミッタのパラメータ(CBV)
struct EmitterSphere {
	Vector3  translate;      // 位置
	float    radius;         // 射出半径
	uint32_t count;          // 1回の射出数
	float    frequency;      // 射出間隔(秒)
	float    frequencyTime;  // 射出間隔調整用時間
	uint32_t emit;           // 射出許可(このフレームにEmit CSが発生させるか)
};

struct PerFrame {
	float time;
	float deltaTime;
	float padding[2];
};

// GPUParticleConfigをCBVの16バイト境界に合わせて詰め直したもの
// (cbufferではfloat3の直後に別のfloat3を置けないため1レジスタずつパディングする)
struct GPUParticleConfigForGPU {
	Vector3 minScale;     float pad0;
	Vector3 maxScale;     float pad1;
	Vector3 minVelocity;  float pad2;
	Vector3 maxVelocity;  float pad3;
	Vector3 acceleration; float pad4;
	Vector4 startColor;
	Vector4 endColor;
	float lifeTimeMin;
	float lifeTimeMax;
	float endScaleRatio;
	float pad5;
};
