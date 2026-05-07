#include "Particle.hlsli"

static const uint kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1024, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        if (gParticles[particleIndex].color.a != 0)
        {
            int freeListIndex;
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity;
            gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
            float alpha = 1.0f - gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime;
            gParticles[particleIndex].color.a = saturate(alpha);
            
            if (gParticles[particleIndex].color.a == 0) {
                gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
                int32_t freeListIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
                if ((freeListIndex + 1) < kMaxParticles) {
                    gFreeList[freeListIndex + 1] = particleIndex;
                } else {
                    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
                }
            }
        }
    }
}