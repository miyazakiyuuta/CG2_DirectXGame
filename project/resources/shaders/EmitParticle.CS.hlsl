#include "GPUParticle.hlsli"
#include "RandomGenerator.hlsl"

RWStructuredBuffer<GPUParticle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<ParticleConfig> gConfig : register(b2);

// 1スレッド=1粒。count分のスレッドがDispatchされ、各スレッドがフリーリストから空きスロットを取り合う
[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (gEmitter.emit == 0) {
        return;
    }
    if (gEmitter.count <= DTid.x) {
        return;
    }

    // スレッドごとに異なる乱数列になるようインデックスと時刻からシードを作る
    RandomGenerator generator;
    generator.seed = (float3(DTid.x, DTid.x * 0.31f, DTid.x * 0.77f) + gPerFrame.time) * gPerFrame.time;

    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (0 <= freeListIndex && freeListIndex < int(kMaxParticles)) {
        uint particleIndex = gFreeList[freeListIndex];
        GPUParticle particle = (GPUParticle) 0;
        // エミッタ位置を中心とした±radiusの立方体状に発生させる
        particle.translate = gEmitter.translate + (generator.Generate3d() * 2.0f - 1.0f) * gEmitter.radius;
        particle.baseScale = lerp(gConfig.minScale, gConfig.maxScale, generator.Generate3d());
        particle.scale = particle.baseScale;
        particle.velocity = lerp(gConfig.minVelocity, gConfig.maxVelocity, generator.Generate3d());
        particle.lifeTime = lerp(gConfig.lifeTimeMin, gConfig.lifeTimeMax, generator.Generate1d());
        particle.currentTime = 0.0f;
        particle.color = gConfig.startColor;
        gParticles[particleIndex] = particle;
    } else {
        // 空きが無かったので減らし過ぎたカウンタを戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
