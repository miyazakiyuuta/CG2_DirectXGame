#include "effect/GPUParticleEmitter.h"
#include "effect/GPUParticleManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

GPUParticleEmitter::GPUParticleEmitter(const std::string& groupName, const std::string& textureFilePath,
	const GPUParticleConfig& config)
	: groupName_(groupName), config_(config) {
	GPUParticleManager::GetInstance()->CreateParticleGroup(groupName_, textureFilePath, config_);
	GPUParticleManager::GetInstance()->RegisterEmitter(this);
}

GPUParticleEmitter::~GPUParticleEmitter() {
	GPUParticleManager::GetInstance()->UnregisterEmitter(this);
}

void GPUParticleEmitter::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Checkbox("active", &isActive_);
	ImGui::DragFloat3("position", &position_.x, 0.01f);
	ImGui::DragFloat("radius", &radius_, 0.1f, 0.0f, 100.0f);
	int count = static_cast<int>(emitCount_);
	if (ImGui::DragInt("emitCount", &count, 1, 0, static_cast<int>(GPUParticleManager::kMaxParticles))) {
		emitCount_ = static_cast<uint32_t>(count < 0 ? 0 : count);
	}
	ImGui::DragFloat("frequency", &frequency_, 0.01f, 0.0f, 10.0f);
	ImGui::SeparatorText("config");
	GPUParticleManager::DrawConfigImGui(config_);
#endif
}
