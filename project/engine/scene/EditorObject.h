#pragma once
#include "math/Transform.h"

#include <functional>
#include <string>

// Hierarchy/Inspector/ギズモ/シーン保存読込が共有する「シーン内オブジェクト」1件。
// name と transform はシリアライズが全構成で使うため、USE_IMGUI には依存させない。
// (drawInspector は Debug 構成の Inspector からのみ呼ばれる)
struct EditorObject {
	std::string name;
	Transform* transform = nullptr;
	bool pickable = true;    // Sceneビューのクリック選択の対象にするか(全天を覆う背景等はfalse)
	float pickRadius = 0.0f; // 判定球の半径。0以下ならscaleの最大成分から自動算出
	bool scaleEditable = true; // scaleの編集を許可するか(カメラ等、scaleが描画を歪ませる物はfalse)
	bool disabled = false;     // 無効フラグの表示用現在値(Hierarchyのグレー表示・チェックボックスが参照する)
	std::function<void()> drawInspector; // 型ごとの追加UI。Transform以外を編集したい物だけ設定する
	// 無効フラグ切替時に呼ばれる(引数は新しいdisabled値)。実体の生成/破棄などの副作用は登録側が行う。
	// 未設定なら切替UI自体を出さない(C++直書きの固定オブジェクトは無効化の対象外)
	std::function<void(bool)> onSetDisabled;
};
