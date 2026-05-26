#include "XPOrb.h"

#include "utility/Random.h"

#include <algorithm>
#include <cmath>

void XPOrb::Init(const Vector3& pos, int val)
{
    active = true;
    position = pos;
    value = val;
    currentTime = 0.0f;
    lifeTime = 4.0f;
    alpha = 1.0f;

    velocity.x = Random::GetFloat(-0.5f, 0.5f);
    velocity.y = Random::GetFloat(0.5f, 1.5f);
    velocity.z = Random::GetFloat(-0.5f, 0.5f);

    const float kVisualScaleMultiplier = 5.0f;
    scale = (0.1f + (std::min)(1.0f, float(value) / 10.0f) * 0.25f) * kVisualScaleMultiplier;

    // ランダムで初期時間をずらして、オーブの動きにバラつきを出す
    currentTime = Random::GetFloat(0.0f, 2.0f * 3.14159f);

    // デフォルトでは寿命なしで、プレイヤーに吸われるまで存在する
    finiteLife = false;

    onGround = false;
    hasBounced = false;
}

void XPOrb::FillInstanceData(ParticleManager::InstanceData& out, const Matrix4x4& cameraMatrix, const Matrix4x4& viewProj) const
{
    Matrix4x4 backToFrontMatrix = Matrix4x4::RotateY(3.14159265f);
    Matrix4x4 billboardMatrix = backToFrontMatrix * cameraMatrix;
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;
    Matrix4x4 scaleMat = Matrix4x4::Scale({ scale, scale, scale });
    Matrix4x4 translateMat = Matrix4x4::Translate(position);
    Matrix4x4 world = scaleMat * billboardMatrix * translateMat;
    out.world = world;
    out.wvp = world * viewProj;
    out.color = {1.0f, 1.0f, 0.6f, alpha};
}

int XPOrb::Update(float dt, const Vector3& attractPos)
{
    if (!active)
        return 0;

    currentTime += dt;
    velocity.y -= 0.5f * dt;

    // バウンドのためのbob（見た目の上下動）を追加。
    // 物理的な位置には直接影響しないが、地面にいるときの微妙な動きを演出
    const float bobFreq = 4.0f;
    const float bobAmp = 0.25f;
    float bob = std::sin(currentTime * bobFreq) * bobAmp;

    position.x += velocity.x * dt;

    float halfHeight = scale * 0.5f;
    if (!onGround) {
        position.y += velocity.y * dt + bob * dt;
    } else {
        // 高さは地面に固定、bobは見た目の上下動のみ
        position.y = groundY + halfHeight;
    }

    position.z += velocity.z * dt;

    if (!onGround) {
        if (position.y <= groundY) {
            if (!hasBounced) {
                position.y = groundY + halfHeight + 0.25f;
                velocity.y = 1.2f;
                velocity.x *= 0.3f;
                velocity.z *= 0.3f;
                hasBounced = true;
            } else {
                position.y = groundY + halfHeight;
                onGround = true;
                velocity.x *= 0.2f;
                velocity.y = 0.0f;
                velocity.z *= 0.2f;
            }
        }
    } else {
        velocity.x *= 0.8f;
        velocity.z *= 0.8f;

        // ゼロ以下にならないように
        if (std::fabs(velocity.x) < 0.01f)
            velocity.x = 0.0f;
        if (std::fabs(velocity.z) < 0.01f)
            velocity.z = 0.0f;

        position.y = groundY + halfHeight;
    }

    float dx = attractPos.x - position.x;
    float dy = attractPos.y - position.y;
    float dz = attractPos.z - position.z;
    float distSq = dx * dx + dy * dy + dz * dz;
    float dist = std::sqrt(distSq);

    // プレイヤーに引き寄せられる処理
    const float attractRadius = 6.0f;
    if (dist < attractRadius) {
        float inv = (dist > 0.0001f) ? 1.0f / dist : 0.0f;
        Vector3 dir = { dx * inv, dy * inv, dz * inv };

        const float maxAttractSpeed = 8.0f;
        const float slowRadius = 1.2f;
        float desiredSpeed = maxAttractSpeed * (1.0f - (dist / attractRadius));

        if (dist < slowRadius) {
            desiredSpeed *= (dist / slowRadius);
        }

        Vector3 desiredVel = { dir.x * desiredSpeed, dir.y * desiredSpeed, dir.z * desiredSpeed };

        float steerFactor = (dt * 8.0f < 1.0f) ? (dt * 8.0f) : 1.0f;
        velocity.x = velocity.x * (1.0f - steerFactor) + desiredVel.x * steerFactor;
        velocity.y = velocity.y * (1.0f - steerFactor) + desiredVel.y * steerFactor;
        velocity.z = velocity.z * (1.0f - steerFactor) + desiredVel.z * steerFactor;

        Vector3 up = { 0.0f, 1.0f, 0.0f };
        Vector3 tangent = Vector3::Cross(dir, up);
        float tlen = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (tlen > 1e-6f) {
            tangent.x /= tlen;
            tangent.y /= tlen;
            tangent.z /= tlen;
            const float spinStrength = 0.6f;
            velocity.x += tangent.x * spinStrength * (1.0f - dist / attractRadius) * dt;
            velocity.y += tangent.y * spinStrength * (1.0f - dist / attractRadius) * dt;
            velocity.z += tangent.z * spinStrength * (1.0f - dist / attractRadius) * dt;
        }

        velocity.x *= 0.92f;
        velocity.y *= 0.92f;
        velocity.z *= 0.92f;

        if (onGround) {
            onGround = false;
            hasBounced = true;
        }
    }

    // 地面にいる状態で引き寄せが働いているなら、地面から離して自由に動けるようにする
    if (onGround && velocity.y > 0.3f) {
        onGround = false;
    }

    // lifeTime 経過でフェードアウトする処理
    if (finiteLife) {
        if (currentTime >= lifeTime) {
            active = false;
            return 0;
        }
        alpha = 1.0f - (currentTime / lifeTime);
    } else {
        alpha = 1.0f;
    }

    const float collectDist = 0.8f;
    if (dist < collectDist) {
        active = false;
        return value;
    }

    return 0;
}
