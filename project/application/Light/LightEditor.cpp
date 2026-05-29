#include "LightEditor.h"

#include "LightManager.h"

#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void LightEditor::Initialize(const std::string& filePath)
{
	filePath_ = filePath;

	std::snprintf(
		filePathBuffer_,
		sizeof(filePathBuffer_),
		"%s",
		filePath_.c_str()
	);
}

void LightEditor::SetMessage(const std::string& message)
{
	message_ = message;
}

void LightEditor::DrawImGui()
{
#ifdef USE_IMGUI

	LightManager* lightManager = LightManager::GetInstance();

	ImGui::Begin("Light Editor");

	if (ImGui::InputText("Light JSON", filePathBuffer_, sizeof(filePathBuffer_))) {
		filePath_ = filePathBuffer_;
		lightManager->SetFilePath(filePath_);
	}

	if (ImGui::Button("Load")) {
		const bool loaded = lightManager->LoadFromFile(filePath_);
		SetMessage(loaded ? "Loaded." : "Load failed. Fallback light was used.");
	}

	ImGui::SameLine();

	if (ImGui::Button("Save")) {
		const bool saved = lightManager->SaveToFile(filePath_);
		SetMessage(saved ? "Saved." : "Save failed.");
	}

	ImGui::SameLine();

	if (ImGui::Button("Apply")) {
		lightManager->ApplyToObject3dCommon();
		SetMessage("Applied.");
	}

	if (ImGui::Button("Add Point Light")) {
		const bool added = lightManager->AddDefaultPointLight();
		SetMessage(added ? "Point light added." : "Cannot add more point lights.");
	}

	if (!message_.empty()) {
		ImGui::Text("%s", message_.c_str());
	}

	auto& lights = lightManager->GetPointLightsForEdit();

	ImGui::Separator();
	ImGui::Text("PointLights: %d / %d", static_cast<int>(lights.size()), kMaxPointLights);

	for (size_t i = 0; i < lights.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		auto& light = lights[i];

		std::string headerName = light.name + "##PointLightHeader";
		if (ImGui::TreeNode(headerName.c_str())) {
			bool changed = false;

			char nameBuffer[128] = {};
			std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", light.name.c_str());

			if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
				light.name = nameBuffer;
			}

			changed |= ImGui::Checkbox("Enabled", &light.enabled);
			changed |= ImGui::ColorEdit4("Color", &light.color.x);
			changed |= ImGui::DragFloat3("Position", &light.position.x, 0.1f);
			changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 100.0f);
			changed |= ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.001f, 100000.0f);
			changed |= ImGui::DragFloat("Decay", &light.decay, 0.01f, 0.0f, 10.0f);

			if (changed) {
				lightManager->ApplyToObject3dCommon();
			}

			ImGui::Separator();

			if (ImGui::Button("Up")) {
				lightManager->MovePointLightUp(i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			ImGui::SameLine();

			if (ImGui::Button("Down")) {
				lightManager->MovePointLightDown(i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			ImGui::SameLine();

			if (ImGui::Button("Delete")) {
				lightManager->RemovePointLight(i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::End();

#endif
}