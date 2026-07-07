#include "EditorPanels.h"

#ifdef USE_IMGUI
#include <imgui.h>

void EditorPanels::DrawHierarchy(const std::vector<EditorObject>& objects, int& selectedIndex) {
	for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
		if (ImGui::Selectable(objects[i].name.c_str(), selectedIndex == i)) {
			selectedIndex = i;
		}
	}
}

void EditorPanels::DrawInspector(const std::vector<EditorObject>& objects, int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(objects.size())) {
		ImGui::TextDisabled("No selection");
		return;
	}
	const EditorObject& object = objects[selectedIndex];
	ImGui::Text("%s", object.name.c_str());

	// 共通部: 全オブジェクトが持つTransformの編集(ギズモと同じ実体を指すため即座に連動する)
	if (object.transform) {
		ImGui::SeparatorText("Transform");
		ImGui::DragFloat3("Position", &object.transform->translate.x, 0.01f);
		ImGui::DragFloat3("Rotation", &object.transform->rotate.x, 0.01f);
		if (object.scaleEditable) {
			ImGui::DragFloat3("Scale", &object.transform->scale.x, 0.01f);
		}
	}

	// 型別部: 登録側が設定した追加UI(カメラのFovY等)
	if (object.drawInspector) {
		ImGui::SeparatorText("Details");
		object.drawInspector();
	}
}
#endif
