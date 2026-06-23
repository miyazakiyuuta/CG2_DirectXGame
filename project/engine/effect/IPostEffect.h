#pragma once
#include <cstdint>

class DirectXCommon;
class SrvManager;

class IPostEffect {
public:
    virtual ~IPostEffect() = default;

    virtual void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) = 0;

    virtual void Update(float deltaTime) {}

    virtual void Draw(uint32_t srcSrvIndex) = 0;

    virtual bool IsSeparable() const { return false; }

    virtual uint32_t RenderFirstPass(uint32_t srcSrvIndex) { return srcSrvIndex; }

    virtual void DrawImGui() {}

    bool enabled = true;
    const char* name = "";
};