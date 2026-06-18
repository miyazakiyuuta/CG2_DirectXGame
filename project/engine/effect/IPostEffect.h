#pragma once
#include <cstdint>

class DirectXCommon;
class SrvManager;

class IPostEffect {
public:
    virtual ~IPostEffect() = default;

    virtual void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) = 0;

    virtual void Draw(uint32_t srcSrvIndex) = 0;

    virtual void DrawImGui() {}

    bool enabled = true;
    const char* name = "";
};