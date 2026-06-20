#include "effect/ImplosionEffect.h"
#include "effect/ParticleManager.h"
#include "effect/RingManager.h"

#include <cmath>
#include <numbers>
#include <random>

namespace {
    const char* kGroup = "implosion";
    const float kTwoPi = 2.0f * std::numbers::pi_v<float>;

    // 腕に少し太さを出すための微小ジッタ
    float Jitter(float range) {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> d(-range, range);
        return d(rng);
    }
}

void ImplosionEffect::Initialize() {
    // 爆発と同じ加算スプライトでOK。独立調整のため別グループにする
    ParticleManager::GetInstance()->CreateParticleGroup(
        kGroup, "resources/circle.png", BlendMode::Add);
}

void ImplosionEffect::Trigger(const Vector3& center) {
    center_ = center;
    timer_ = 0.0f;
    spawnAngle_ = 0.0f;
    playing_ = true;
    ringTimer_ = 0.0f;
}

void ImplosionEffect::Update(float deltaTime) {
    if (!playing_) { return; }

    timer_ += deltaTime;

    ringTimer_ += deltaTime;
    while (ringTimer_ >= ringLifeTime_) {
        ringTimer_ -= ringLifeTime_;
        EmitCollapsingRing(center_);
    }

    if (timer_ >= duration_) {
        playing_ = false;
        return;
    }

    // arms_ 本の腕を、それぞれ spawnAngle_ から等間隔の角度で撒く。
    // spawnAngle_ が回ることで腕が回転し、渦の見た目になる。
    for (int a = 0; a < arms_; ++a) {
        float armTheta = spawnAngle_ + a * (kTwoPi / arms_);
        for (int p = 0; p < perArm_; ++p) {
            EmitWisp(center_, armTheta + Jitter(0.12f));
        }
    }
    spawnAngle_ += spinPerSecond_ * deltaTime;
}

void ImplosionEffect::EmitWisp(const Vector3& center, float theta) {
    const float c = std::cos(theta);
    const float s = std::sin(theta);

    // カメラに正対するXY平面の殻の上に発生（半径に微小ジッタ）
    float t = Jitter(1.0f) * 0.5f + 0.5f;
    t = t * t;
    const float r = innerRadius_ + (spawnRadius_ - innerRadius_) * t;
    Vector3 pos = { center.x + c * r, center.y + s * r, center.z };

    // radialOut = (c, s, 0) / tangent = (-s, c, 0)
    // velocity  = -radialOut * vIn + tangent * vTan
    Vector3 velocity = {
        -c * inwardSpeed_ + (-s) * tangentialSpeed_,
        -s * inwardSpeed_ + (c)*tangentialSpeed_,
        0.0f
    };

    ParticleConfig cfg{};
    // 中心に到達する頃に寿命が尽きるように。drag=0前提なので r/vIn が到達時間
    cfg.lifeTime = (r / inwardSpeed_) * 0.95f;
    cfg.minScale = { 0.3f, 0.3f, 0.3f };
    cfg.maxScale = { 0.6f, 0.6f, 0.6f };
    cfg.useRadialVelocity = false;                 // 明示速度を使う
    cfg.minVelocity = cfg.maxVelocity = velocity;  // min==maxで確定値に
    cfg.drag = 0.0f;                               // 減速させない（中心まで届かせる）
    cfg.startScaleMul = 1.0f;  cfg.endScaleMul = 0.15f;  // 中心へ吸われて点になる
    cfg.useColorLerp = true;
    cfg.startColor = { 0.5f, 0.8f, 1.0f, 0.9f };   // 青白い光
    cfg.endColor = { 0.8f, 0.95f, 1.0f, 0.0f };  // 中心で消える

    ParticleManager::GetInstance()->Emit(kGroup, pos, cfg, 1);
}

void ImplosionEffect::EmitCollapsingRing(const Vector3& center) {
    RingConfig r{};
    r.lifeTime = ringLifeTime_;
    r.startScale = spawnRadius_;                 // 大きく出して
    r.endScale = 0.1f;                         // 中心へ収縮
    r.startColor = { 0.7f, 0.9f, 1.0f, 0.8f };
    r.endColor = { 0.5f, 0.85f, 1.0f, 0.0f };
    r.isBillboard = true;
    RingManager::GetInstance()->Emit(center, { 0,0,0 }, r);
}