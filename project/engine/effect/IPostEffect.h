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

    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    // EffectManager::FindEffect の検索キー。生成時に決まり、後から変えられない
    const char* GetName() const { return name_; }

protected:
    // 派生クラスは自分の名前を渡す(例: Vignette() : IPostEffect("Vignette") {})
    explicit IPostEffect(const char* name) : name_(name) {}

private:
    bool enabled_ = true;
    const char* name_ = "";
};