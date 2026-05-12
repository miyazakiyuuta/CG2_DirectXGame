#pragma once
#include "math/Vector3.h"
#include <cmath>

enum class AbilityId : int;

class XPOrb {
public:
    XPOrb() = default;
    void Init(const Vector3& pos, int value);
    void SetAbility(AbilityId a) { ability_ = a; }
    AbilityId GetAbility() const { return ability_; }
    int Update(float dt, const Vector3& attractPos);

    bool IsActive() const { return active; }
    const Vector3& GetPosition() const { return position; }
    float GetAlpha() const { return alpha; }
    float GetScale() const { return scale; }
    int GetValue() const { return value; }

    void SetGroundY(float y) { groundY = y; }
    void SetFiniteLife(bool v) { finiteLife = v; }

private:
    bool active = false;
    Vector3 position = { 0, 0, 0 };
    Vector3 velocity = { 0, 0, 0 };
    float lifeTime = 2.5f;
    float currentTime = 0.0f; 
    float alpha = 1.0f;
    bool finiteLife = false;

    float groundY = 0.0f;
    bool onGround = false;

    bool hasBounced = false;
    float scale = 0.2f;
    int value = 1;

    AbilityId ability_ = (AbilityId)0;
};

// XPOrbSystem will implement gameplay behavior.
// XPOrb itself keeps only per-orb state.


