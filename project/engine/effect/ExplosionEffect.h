#pragma once
#include "math/Vector3.h"

class ExplosionEffect {
public:
    void Initialize();                  // 使用するパーティクルグループを生成
    void Trigger(const Vector3& pos);   // 1発の爆発を発火
    void Update(float deltaTime);       // ループ再生用(任意)

    bool  loop_ = false;       // trueで自動的に繰り返す(ショーケース用)
    float loopInterval_ = 1.6f;

private:
    void EmitFlash(const Vector3& pos);
    void EmitCore(const Vector3& pos);
    void EmitSparks(const Vector3& pos);
    void EmitShockwave(const Vector3& pos);
    void EmitEmbers(const Vector3& pos);
    void EmitSmoke(const Vector3& pos);

    float   loopTimer_ = 0.0f;
    Vector3 lastPos_{};
};