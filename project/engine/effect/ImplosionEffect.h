#pragma once
#include "math/Vector3.h"

class ImplosionEffect {
public:
    void Initialize();                 // パーティクルグループ生成
    void Trigger(const Vector3& center);  // 吸い込み開始
    void Update(float deltaTime);      // 毎フレーム呼ぶ

    bool IsPlaying() const { return playing_; }

    // 調整用パラメータ
    float duration_ = 2.0f;   // 吸い込みが続く時間[s]
    float spawnRadius_ = 5.0f;  // 粒子が発生する円の半径
    float inwardSpeed_ = 8.0f;   // 中心へ向かう速度 vIn
    float tangentialSpeed_ = 5.0f;   // 接線方向の速度 vTan（渦の強さ。0で渦なし）
    float spinPerSecond_ = 6.0f;   // 発生角の回転速度[rad/s]（渦の腕の本数感）
    int   arms_ = 6;      // 同時に伸びる腕の数
    int   perArm_ = 5;      // 1腕あたり1フレームの発生数
    float ringLifeTime_ = 0.5f;   // 1枚の寿命（収縮にかける時間）
    float ringInterval_ = 0.12f;  // 何秒ごとに出すか（小さいほど密）
    float innerRadius_ = 1.0f;

private:
    void EmitWisp(const Vector3& center, float theta);
    void EmitCollapsingRing(const Vector3& center);

    bool    playing_ = false;
    float   timer_ = 0.0f;
    float   spawnAngle_ = 0.0f;
    Vector3 center_{};

    float ringTimer_ = 0.0f;
};