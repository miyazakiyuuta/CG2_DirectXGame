#include "Particle.hlsli"

// GPUパーティクル共通定義。C++側(engine/effect/GPUParticle.h)の構造体と必ず一致させること

// 1グループあたりの最大粒子数(GPUParticleManager::kMaxParticlesと必ず一致させる)
static const uint kMaxParticles = 102400;

// 粒子1粒(StructuredBuffer。全て0=死亡状態で、scale=0のため描画されない)
struct GPUParticle {
    float3 translate;
    float3 scale;
    float3 baseScale; // 生成時スケール(寿命補間の基準)
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

struct PerView {
    float4x4 viewProjection;
    float4x4 billboardMatrix;
};

// 継続放出エミッタのパラメータ
struct EmitterSphere {
    float3 translate; // 位置
    float radius; // 射出半径
    uint count; // 1回の射出数
    float frequency; // 射出間隔(秒)
    float frequencyTime; // 射出間隔調整用時間
    uint emit; // 射出許可
};

struct PerFrame {
    float time;
    float deltaTime;
};

// 粒子の生成・挙動パラメータ。min/maxは乱数範囲
struct ParticleConfig {
    float3 minScale;
    float3 maxScale;
    float3 minVelocity;
    float3 maxVelocity;
    float3 acceleration;
    float4 startColor;
    float4 endColor;
    float lifeTimeMin;
    float lifeTimeMax;
    float endScaleRatio;
};
