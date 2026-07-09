#include "GPUParticle.hlsli"

RWStructuredBuffer<GPUParticle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

// 全スロットを死亡状態にしてフリーリストを構築する(グループ生成時に1回だけ流す)
[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint particleIndex = DTid.x;
    if (particleIndex < kMaxParticles) {
        gParticles[particleIndex] = (GPUParticle) 0; // 全て0=死亡(scale=0で描画されない)
        gFreeList[particleIndex] = particleIndex;
        if (particleIndex == 0) {
            // freeListIndexは「最後の有効エントリの添字」。全スロット空きなのでkMax-1
            gFreeListIndex[0] = kMaxParticles - 1;
        }
    }
}
