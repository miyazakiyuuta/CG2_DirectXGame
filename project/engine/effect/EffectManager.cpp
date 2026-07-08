#include "EffectManager.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void EffectManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
    uint32_t width, uint32_t height) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // ピンポン用RTVスロットは自動採番で確保する(手動採番による衝突を防ぐ)
    for (int i = 0; i < 2; ++i) {
        pingPong_[i] = std::make_unique<RenderTarget>();
        pingPong_[i]->Create(dxCommon->GetDevice(), srvManager,
            dxCommon->GetRTVCPUDescriptorHandle(dxCommon->AllocateRtvIndex()), width, height);
    }
}

void EffectManager::AddEffect(std::unique_ptr<IPostEffect> effect) {
    effect->Initialize(dxCommon_, srvManager_);
    effects_.push_back(std::move(effect));
}

uint32_t EffectManager::Apply(uint32_t inputSrvIndex) {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    uint32_t currentSrv = inputSrvIndex;
    int writeIndex = 0;

    for (auto& effect : effects_) {
        if (!effect->enabled) {
            continue;
        }

        RenderTarget* target = pingPong_[writeIndex].get();

        if (effect->IsSeparable()) {
            uint32_t midSrv = effect->RenderFirstPass(currentSrv);
            target->BeginRenderNoDepth(commandList);
            effect->Draw(midSrv);
            target->EndRender(commandList);
        } else {
            target->BeginRenderNoDepth(commandList);
            effect->Draw(currentSrv);
            target->EndRender(commandList);
        }

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
