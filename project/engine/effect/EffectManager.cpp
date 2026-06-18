#include "EffectManager.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void EffectManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
    uint32_t rtvIndex0, uint32_t rtvIndex1, uint32_t width, uint32_t height) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    pingPong_[0] = std::make_unique<RenderTarget>();
    pingPong_[0]->Create(dxCommon->GetDevice(), srvManager,
        dxCommon->GetRTVCPUDescriptorHandle(rtvIndex0), width, height);

    pingPong_[1] = std::make_unique<RenderTarget>();
    pingPong_[1]->Create(dxCommon->GetDevice(), srvManager,
        dxCommon->GetRTVCPUDescriptorHandle(rtvIndex1), width, height);
}

void EffectManager::AddEffect(std::unique_ptr<IPostEffect> effect) {
    effect->Initialize(dxCommon_, srvManager_);
    effects_.push_back(std::move(effect));
}

uint32_t EffectManager::Apply(uint32_t inputSrvIndex) {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);

    uint32_t currentSrv = inputSrvIndex;
    int writeIndex = 0;

    for (auto& effect : effects_) {
        if (!effect->enabled) {
            continue;
        }

        RenderTarget* target = pingPong_[writeIndex].get();

        target->BeginRender(commandList, dsvHandle);
        effect->Draw(currentSrv);
        target->EndRender(commandList);

        currentSrv = target->GetSrvIndex();
        writeIndex = 1 - writeIndex;
    }

    return currentSrv;
}

void EffectManager::Update(float deltaTime) {
    for (auto& effect : effects_) {
        effect->Update(deltaTime);
    }
}

void EffectManager::DrawImGui() {
#ifdef USE_IMGUI
    for (auto& effect : effects_) {
        ImGui::PushID(effect->name);
        // エフェクトごとに折りたたみ見出し
        if (ImGui::CollapsingHeader(effect->name)) {
            // ON/OFFチェックボックス（##で内部IDを区別）
            ImGui::Checkbox((std::string("Enable##") + effect->name).c_str(), &effect->enabled);
            // 各エフェクト固有のUI（Monochromeなら色編集）
            effect->DrawImGui();
        }
        ImGui::PopID();
    }
#endif
}

void EffectManager::ResetAll() {
    for (auto& effect : effects_) {
        effect->enabled = false;
    }
}

IPostEffect* EffectManager::FindEffect(const std::string& name) {
    for (auto& e : effects_) {
        if (e->name == name) {
            return e.get();
        }
    }
    return nullptr;
}
