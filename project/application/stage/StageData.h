#pragma once
#include "math/Transform.h"

#include <string>
#include <vector>

// stage.json 1ファイル分のステージデータ(中間表現)。
// StageSerializerが読み書きし、Stageがこれを元にランタイム実体(Object3d群)を構築する。
// エディタ(StageEditor)もこの構造体を編集対象にする予定なので、
// ゲームで使わない情報(disabled等)もここに保持する。
struct StageData {
	// コライダー1個分のパラメータ(AABB)
	struct ColliderData {
		std::string type = "AABB";             // 形状の種別(当面はAABBのみ)
		Vector3 center = { 0.0f, 0.0f, 0.0f }; // オブジェクト中心からのオフセット
		Vector3 size = { 1.0f, 1.0f, 1.0f };   // 各軸の大きさ
	};

	// 静的配置オブジェクト1個分のデータ(type="static")
	// spawn(敵出現)やrail(制御点)はレール実装時にここへ追加する
	struct ObjectData {
		std::string name;           // エディタ上での表示名
		std::string model;          // ModelManagerへ渡すモデルパス(resourcesからの相対。例:"fence/fence.obj")
		Transform transform;        // ゲーム座標系ネイティブ(変換なしでそのまま使う)
		bool disabled = false;      // trueなら実体を生成しない(データとしては保持する)
		bool hasCollider = false;   // コライダーが設定されているか
		ColliderData collider;      // hasColliderがtrueのときのみ有効
	};

	// 配置オブジェクトのリスト
	std::vector<ObjectData> objects;
};
