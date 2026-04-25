#include "Particle.hlsli"

static const uint kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeCounter : register(u1);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles) {
        gParticles[particleIndex] = (Particle) 0;
        gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].color = float4(1.0f, 1.0f, 1.0f, 0.0f);
        if (particleIndex == 0)
        {
            gFreeCounter[0] = 0;
        }
    }
}