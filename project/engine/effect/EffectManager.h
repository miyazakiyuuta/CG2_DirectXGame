#pragma once
#include "base/RenderTarget.h"
#include "effect/IPostEffect.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <string>

class DirectXCommon;
class SrvManager;

class EffectManager {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
                    uint32_t rtvIndex0, uint32_t rtvIndex1,
                    uint32_t width, uint32_t height);

    void AddEffect(std::unique_ptr<IPostEffect> effect);

    uint32_t Apply(uint32_t inputSrvIndex);

	void Update(float deltaTime);

    void DrawImGui();

    void ResetAll();

    std::vector<std::unique_ptr<IPostEffect>>& GetEffects() { return effects_; }

    IPostEffect* FindEffect(const std::string& name);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<RenderTarget> pingPong_[2];
    std::vector<std::unique_ptr<IPostEffect>> effects_;
};