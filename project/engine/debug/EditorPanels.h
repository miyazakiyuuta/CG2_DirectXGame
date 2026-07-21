#pragma once
#ifdef USE_IMGUI
#include "scene/EditorObject.h"

#include <vector>

// Hierarchy/Inspectorパネルの中身を描画するヘルパー。
// TransformGizmoと同様、Begin/Endは呼び出し側(シーン)が行い、ここは現在のウィンドウに描く
namespace EditorPanels {
	/// <summary>
	/// オブジェクト一覧を描画し、クリックで選択を切り替える。
	/// onSetDisabled持ちの行には有効/無効チェックボックスが付く(無効の行は名前をグレー表示)
	/// </summary>
	/// <param name="selectedIndex">選択中の添字。-1 = 選択なし</param>
	void DrawHierarchy(std::vector<EditorObject>& objects, int& selectedIndex);

	/// <summary>
	/// 選択中オブジェクトのTransform(共通部)と drawInspector(型別部)を描画する。
	/// 選択なしのときはその旨を表示する
	/// </summary>
	void DrawInspector(std::vector<EditorObject>& objects, int selectedIndex);
}
#endif
