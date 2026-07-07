#include "effect/ParticleEmitter.h"
#include "effect/ParticleManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

ParticleEmitter::ParticleEmitter(const std::string& groupName, const std::string& textureFilePath,
	const ParticleConfig& config, BlendMode blendMode)
	: groupName_(groupName), config_(config) {
	// グループ作成と既定configの登録をまとめて行う(グループが既にあればconfigのみ上書き)
	ParticleManager::GetInstance()->RegisterEffect(groupName_, textureFilePath, config_, blendMode);
	ParticleManager::GetInstance()->RegisterEmitter(this);
}

ParticleEmitter::~ParticleEmitter() {
	ParticleManager::GetInstance()->UnregisterEmitter(this);
}

void ParticleEmitter::Update(float deltaTime) {
	if (!isActive_ || emitInterval_ <= 0.0f) { return; }

	elapsed_ += deltaTime;
	// フレームが跨いでも発生数が間引かれないようにキャッチアップする
	while (elapsed_ >= emitInterval_) {
		Emit();
		elapsed_ -= emitInterval_;
	}
}

void ParticleEmitter::Emit() {
	ParticleManager::GetInstance()->Emit(groupName_, position_, config_, count_);
}

void ParticleEmitter::EmitAt(const Vector3& position, uint32_t count) {
	ParticleManager::GetInstance()->Emit(groupName_, position, config_, count);
}

void ParticleEmitter::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Checkbox("active", &isActive_);
	ImGui::DragFloat3("position", &position_.x, 0.01f);
	int count = static_cast<int>(count_);
	if (ImGui::DragInt("count", &count, 1, 0, 1024)) {
		count_ = static_cast<uint32_t>(count < 0 ? 0 : count);
	}
	ImGui::DragFloat("emitInterval", &emitInterval_, 0.01f, 0.0f, 10.0f);
	if (ImGui::Button("Emit")) {
		Emit();
	}
	ImGui::SeparatorText("config");
	ParticleManager::DrawConfigImGui(config_);
#endif
}
