#include "EditorPanels.h"

#ifdef USE_IMGUI
#include <imgui.h>

namespace {
	// 有効/無効チェックボックス(チェックON=有効)。切替時はdisabledを書き換えてから通知する
	void DrawActiveCheckbox(EditorObject& object) {
		bool active = !object.disabled;
		if (ImGui::Checkbox("##active", &active)) {
			object.disabled = !active;
			object.onSetDisabled(object.disabled);
		}
	}
}

void EditorPanels::DrawHierarchy(std::vector<EditorObject>& objects, int& selectedIndex) {
	for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
		// Selectableはラベル文字列がIDになるため、同名オブジェクトが並ぶと選択が壊れる。
		// 添字ベースのIDで囲んで名前の重複を許容する
		ImGui::PushID(i);
		if (objects[i].onSetDisabled) {
			DrawActiveCheckbox(objects[i]);
			ImGui::SameLine();
		}
		// 無効中はどれが無効か一目で分かるよう名前をグレー表示する(選択は可能なまま)
		if (objects[i].disabled) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		}
		if (ImGui::Selectable(objects[i].name.c_str(), selectedIndex == i)) {
			selectedIndex = i;
		}
		if (objects[i].disabled) {
			ImGui::PopStyleColor();
		}
		ImGui::PopID();
	}
}

void EditorPanels::DrawInspector(std::vector<EditorObject>& objects, int selectedIndex) {
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(objects.size())) {
		ImGui::TextDisabled("No selection");
		return;
	}
	EditorObject& object = objects[selectedIndex];
	if (object.onSetDisabled) {
		DrawActiveCheckbox(object);
		ImGui::SameLine();
	}
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
