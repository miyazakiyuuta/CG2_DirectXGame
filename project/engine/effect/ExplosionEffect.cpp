#include "effect/ExplosionEffect.h"
#include "effect/ParticleManager.h"
#include "effect/RingManager.h"

#include <numbers>

namespace {
    const char* kGroup = "explosion";
    const float kPi = std::numbers::pi_v<float>;
}

void ExplosionEffect::Initialize() {
    // 閃光・コア・火花は同じ加算スプライト(circle.png)なので1グループで共用
    ParticleManager::GetInstance()->CreateParticleGroup(
        kGroup, "resources/circle.png", BlendMode::Add);
}

void ExplosionEffect::Trigger(const Vector3& pos) {
    lastPos_ = pos;
    EmitFlash(pos);
    EmitCore(pos);
    EmitSparks(pos);
    EmitShockwave(pos);
    EmitEmbers(pos);
    EmitSmoke(pos);
}

void ExplosionEffect::Update(float deltaTime) {
    if (!loop_) { return; }
    loopTimer_ += deltaTime;
    if (loopTimer_ >= loopInterval_) {
        loopTimer_ -= loopInterval_;
        Trigger(lastPos_);
    }
}

void ExplosionEffect::EmitFlash(const Vector3& pos) {
    ParticleConfig c{};
    c.lifeTime = 0.12f;
    c.minScale = c.maxScale = { 6.0f, 6.0f, 6.0f };
    c.minVelocity = c.maxVelocity = { 0,0,0 };
    c.startScaleMul = 0.6f;  c.endScaleMul = 1.7f;   // 一瞬で開く
    c.useColorLerp = true;
    c.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    c.endColor = { 1.0f, 1.0f, 0.9f, 0.0f };       // 白 → 透明
    ParticleManager::GetInstance()->Emit(kGroup, pos, c, 1);
}

void ExplosionEffect::EmitCore(const Vector3& pos) {
    ParticleConfig c{};
    c.lifeTime = 0.7f;
    c.minScale = { 2.0f,2.0f,2.0f };  c.maxScale = { 3.2f,3.2f,3.2f };
    c.useRadialVelocity = true;  c.minSpeed = 0.5f;  c.maxSpeed = 2.5f;
    c.drag = 4.0f;
    c.startScaleMul = 0.4f;  c.endScaleMul = 1.5f;   // 膨らみながら
    c.useColorLerp = true;
    c.startColor = { 1.0f, 0.95f, 0.7f, 1.0f };      // 黄白
    c.endColor = { 1.0f, 0.35f, 0.05f, 0.0f };     // 赤橙 → 透明
    ParticleManager::GetInstance()->Emit(kGroup, pos, c, 8);
}

void ExplosionEffect::EmitSparks(const Vector3& pos) {
    ParticleConfig c{};
    c.lifeTime = 1.1f;
    c.minScale = { 0.25f,0.25f,0.25f };  c.maxScale = { 0.6f,0.6f,0.6f };
    c.useRadialVelocity = true;  c.minSpeed = 6.0f;  c.maxSpeed = 16.0f;
    c.drag = 3.0f;                                   // 飛び出して急減速 = 爆発感
    c.gravity = { 0.0f, -6.0f, 0.0f };               // 落下の弧
    c.startScaleMul = 1.0f;  c.endScaleMul = 0.2f;   // 縮みながら消える
    c.useColorLerp = true;
    c.startColor = { 1.0f, 0.8f, 0.4f, 1.0f };
    c.endColor = { 1.0f, 0.2f, 0.05f, 0.0f };
    ParticleManager::GetInstance()->Emit(kGroup, pos, c, 80);
}

void ExplosionEffect::EmitShockwave(const Vector3& pos) {
    RingConfig r{};
    r.lifeTime = 0.4f;
    r.startScale = 0.5f;  r.endScale = 8.0f;
    r.startColor = { 1.0f, 0.9f, 0.6f, 1.0f };
    r.endColor = { 1.0f, 0.4f, 0.1f, 0.0f };
    r.isBillboard = true;                            // カメラ正対の衝撃波ディスク
    RingManager::GetInstance()->Emit(pos, { 0,0,0 }, r);
}

void ExplosionEffect::EmitEmbers(const Vector3& pos) {
    ParticleConfig c{};
    c.lifeTime = 2.6f;                               // 火花(1.1s)より長く残す
    c.minScale = { 0.12f,0.12f,0.12f };
    c.maxScale = { 0.30f,0.30f,0.30f };
    c.useRadialVelocity = true;
    c.minSpeed = 1.5f;  c.maxSpeed = 5.0f;           // 火花よりゆっくり散る
    c.drag = 1.2f;                                   // すぐ失速して漂う
    c.gravity = { 0.0f, -1.5f, 0.0f };               // ゆっくり落下
    c.startScaleMul = 1.0f;  c.endScaleMul = 0.4f;   // 徐々に小さく
    c.useColorLerp = true;
    c.colorEaseIn = true;                            // ★ 終端まで色を保持して消える
    c.startColor = { 1.0f, 0.55f, 0.18f, 1.0f };     // オレンジの火の粉
    c.endColor = { 0.5f, 0.10f, 0.02f, 0.0f };     // 暗い赤 → 透明
    ParticleManager::GetInstance()->Emit(kGroup, pos, c, 40);
}

void ExplosionEffect::EmitSmoke(const Vector3& pos) {
    ParticleConfig c{};
    c.lifeTime = 3.2f;                               // 最長レイヤー = 余韻の本体
    c.minScale = { 2.0f,2.0f,2.0f };
    c.maxScale = { 3.5f,3.5f,3.5f };
    c.useRadialVelocity = true;
    c.minSpeed = 0.4f;  c.maxSpeed = 1.6f;           // ほぼその場で揺らぐ
    c.drag = 2.0f;
    c.gravity = { 0.0f, 0.8f, 0.0f };                // 熱気がゆっくり上昇
    c.startScaleMul = 0.8f;  c.endScaleMul = 2.4f;   // 膨らみながら薄れる
    c.useColorLerp = true;
    c.colorEaseIn = true;                            // ★ 立ち昇ってから終端でフェード
    c.startColor = { 0.8f, 0.35f, 0.12f, 0.5f };     // ほの暗い橙の熱気(半透明)
    c.endColor = { 0.15f, 0.06f, 0.04f, 0.0f };    // くすんで消える
    ParticleManager::GetInstance()->Emit(kGroup, pos, c, 24);
}