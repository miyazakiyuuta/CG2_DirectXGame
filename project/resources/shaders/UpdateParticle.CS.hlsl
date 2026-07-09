#include "GPUParticle.hlsli"

RWStructuredBuffer<GPUParticle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<ParticleConfig> gConfig : register(b2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint particleIndex = DTid.x;
    if (kMaxParticles <= particleIndex) {
        return;
    }

    GPUParticle particle = gParticles[particleIndex];
    if (particle.lifeTime <= 0.0f) {
        return; // 死亡スロット
    }

    particle.currentTime += gPerFrame.deltaTime;
    if (particle.lifeTime <= particle.currentTime) {
        // 寿命切れ: スロットを空にしてフリーリストへ返す
        gParticles[particleIndex] = (GPUParticle) 0;
        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if ((freeListIndex + 1) < int(kMaxParticles)) {
            gFreeList[freeListIndex + 1] = particleIndex;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1);
        }
        return;
    }

    // 速度系は毎秒量×deltaTimeで積分する(フレームレート非依存)
    particle.velocity += gConfig.acceleration * gPerFrame.deltaTime;
    particle.translate += particle.velocity * gPerFrame.deltaTime;

    // 寿命の進行度(0→1)で色とスケールを線形補間
    float t = saturate(particle.currentTime / particle.lifeTime);
    particle.color = lerp(gConfig.startColor, gConfig.endColor, t);
    particle.scale = particle.baseScale * lerp(1.0f, gConfig.endScaleRatio, t);

    gParticles[particleIndex] = particle;
}
