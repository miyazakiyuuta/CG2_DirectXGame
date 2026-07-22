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
	// spawn(敵出現)は敵実装時にここへ追加する
	struct ObjectData {
		std::string name;           // エディタ上での表示名
		std::string model;          // ModelManagerへ渡すモデルパス(resourcesからの相対。例:"fence/fence.obj")
		Transform transform;        // ゲーム座標系ネイティブ(変換なしでそのまま使う)
		bool disabled = false;      // trueなら実体を生成しない(データとしては保持する)
		bool hasCollider = false;   // コライダーが設定されているか
		ColliderData collider;      // hasColliderがtrueのときのみ有効
	};

	// カメラ調整値。ランタイムの所有者はシーン(GamePlayScene)で、ここはデータの器。
	// 各デフォルトはランタイム側の初期値と一致させること(camera項目の無い旧stage.jsonとの互換のため)
	struct CameraData {
		float fovY = 0.45f;         // Camera::fovY_の初期値(Camera.cpp)と一致
		float railSpeed = 10.0f;    // GamePlayScene::railSpeed_の初期値と一致
		float backDistance = 10.0f; // GamePlayScene::cameraBackDistance_の初期値と一致
	};

	// 配置オブジェクトのリスト
	std::vector<ObjectData> objects;

	// レール制御点(CatmullRomSpline用)。ゲーム座標系ネイティブ。空=レールなし
	std::vector<Vector3> rail;

	// カメラ調整値(省略可)。hasCamera=falseならstage.jsonに未記載で、
	// シーンはApplyせずライブ値を保つ(「ファイルに無ければ何もしない」の流儀)
	bool hasCamera = false;
	CameraData camera;
};
