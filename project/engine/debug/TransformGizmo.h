#pragma once
#ifdef USE_IMGUI
#include "scene/EditorObject.h"

#include <vector>

class Camera;

// Sceneビュー上でTransformを操作するギズモ(ImGuizmo)のヘルパー
namespace TransformGizmo {
	/// <summary>
	/// 操作モード(Translate/Rotate/Scale)切り替えのラジオボタンを現在のImGuiウィンドウに描画
	/// </summary>
	void DrawOperationSelector();

	/// <summary>
	/// Sceneビュー上の左クリックでレイピッキングし選択を更新する。
	/// ギズモ操作中は何もしない。何もない場所のクリックは選択解除(-1)
	/// </summary>
	void PickBySceneClick(const Camera& camera, const std::vector<EditorObject>& objects, int& selectedIndex);

	/// <summary>
	/// Sceneウィンドウ上にギズモを描画し、ドラッグ中は対象のSRTを書き換える。
	/// scaleEditable=false の対象ではScaleモードでもTranslateとして動作する
	/// </summary>
	/// <returns>SRTが変更されたら true</returns>
	bool Manipulate(const Camera& camera, EditorObject& object);
}
#endif
