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

	// 配置オブジェクトの種別。文字列比較を各所に散らさないため列挙で持ち、
	// JSONとの相互変換はStageSerializerに閉じ込める
	enum class ObjectType {
		Static, // 静的配置(地形・建造物など)。モデルをその場に置くだけ
		Spawn,  // 敵の発生地点。モデルは持たず、種別(enemy)と発生する進行度(railDistance)を持つ
		Light,  // 点光源(照明・家電の待機ランプなど)。モデルは持たず、位置(transform.translate)と光の性質(light)を持つ
	};

	// 点光源1個分の性質(ObjectType::Lightのときのみ有効)。位置はObjectDataのtransform.translate
	struct LightData {
		Vector3 color = { 1.0f, 0.55f, 0.2f }; // 色(既定は豆電球の橙)
		float intensity = 1.2f;  // 強さ
		float radius = 150.0f;   // 届く最大距離[単位=cm]。これより遠くは照らさない
		float decay = 1.5f;      // 減衰の強さ(大きいほど中心だけが明るい)
	};

	// 配置オブジェクト1個分のデータ。
	// StaticとSpawnを別配列に分けず1つのobjects[]で扱うことで、Hierarchy/Inspector/ギズモ/
	// 追加・複製・削除・無効フラグといったエディタ機能がそのまま両方に効く
	struct ObjectData {
		ObjectType type = ObjectType::Static;
		std::string name;           // エディタ上での表示名
		std::string model;          // ModelManagerへ渡すモデルパス(resourcesからの相対。例:"fence/fence.obj")
		Transform transform;        // ゲーム座標系ネイティブ(変換なしでそのまま使う)
		bool disabled = false;      // trueなら実体を生成しない(データとしては保持する)
		bool hasCollider = false;   // コライダーが設定されているか
		ColliderData collider;      // hasColliderがtrueのときのみ有効

		// --- type==Spawn のときのみ有効 ---
		// 敵の種別。EnemySpawnerが「種別→モデル」に対応付ける(データにモデルパスを持たせない)
		std::string enemy = "frog";
		// この進行度[m]をプレイヤーが超えたら発生する。出現位置はtransform.translate
		float railDistance = 0.0f;

		// --- type==Light のときのみ有効 ---
		LightData light;
	};

	// ステージ全体の光(夜の雰囲気)。点光源はobjects[]のLightで、ここは位置を持たない光をまとめる。
	// 各既定値はエンジンの初期値(Object3dCommon::ResetLights=昼の見た目)と一致させる
	// (lighting項目はあるが一部の値が無い、という手編集のときに昼の値へ落ちるように)
	struct LightingData {
		Vector3 ambientColor = { 0.0f, 0.0f, 0.0f };     // 環境光(どの光も届かない面にも乗る明るさ)
		Vector3 moonColor = { 1.0f, 1.0f, 1.0f };        // 月明かり(平行光源)の色
		Vector3 moonDirection = { 0.0f, -1.0f, 0.0f };   // 月明かりの向き(光が進む向き。正規化は使う側で行う)
		float moonIntensity = 1.0f;                      // 月明かりの強さ
		// ドローンのライト(スポットライト)。自機から照準の先へ向ける(向きはシーンが毎フレーム決める)
		Vector3 headlightColor = { 1.0f, 1.0f, 1.0f };
		float headlightIntensity = 0.0f;   // 0=消灯(エンジンの初期値と同じ)
		float headlightDistance = 400.0f;  // 届く最大距離[単位=cm]
		float headlightAngle = 0.384f;     // 円錐の半分の角度[rad](約22度)。これより外は照らさない
		float headlightFalloffStart = 0.209f; // ここ[rad](約12度)から外側へ向けて暗くなる(縁のぼかし)
		float headlightDecay = 1.2f;       // 距離による減衰の強さ
	};

	// カメラ調整値。ランタイムの所有者はシーン(GamePlayScene)で、ここはデータの器。
	// 各デフォルトはランタイム側の初期値と一致させること(camera項目の無い旧stage.jsonとの互換のため)
	struct CameraData {
		float fovY = 0.45f;         // Camera::fovY_の初期値(Camera.cpp)と一致
		float railSpeed = 10.0f;    // GamePlayScene::railSpeed_の初期値と一致
		float backDistance = 10.0f; // GamePlayScene::cameraBackDistance_の初期値と一致
		// 描画する奥行きの範囲。1単位=1cmなので、家の中を見渡すには奥を長く取る(既定のままだと1m先で切れる)。
		// 24bitの深度バッファでは奥行きの細かさが手前の値でほぼ決まるので、奥を伸ばすときは手前も上げる
		float nearClip = 0.1f;   // Camera::nearClip_の初期値と一致
		float farClip = 100.0f;  // Camera::farClip_の初期値と一致
	};

	// 区間(ステージの区切り)1個分。進行度[m]でステージを分け、「区間ごとの目標(取り逃しなし)」と
	// 進行ゲージの区切りに使う。区間に属する敵は、SpawnPointのrailDistanceがどの区間に入るかで決まる。
	// 区間の中身(どんな敵を出すか)はSpawnPointの配置で決まるので、ここは区切りの情報だけを持つ
	struct SectionData {
		// エディタ上での表示名(例: "Kitchen")。ImGuiは日本語フォントを読み込んでいないので英数字で付ける
		std::string name;
		float startDistance = 0.0f; // この区間が始まる進行度[m]。終わりは次の区間のstartDistance(最後の区間は終点)
	};

	// 配置オブジェクトのリスト
	std::vector<ObjectData> objects;

	// 区間のリスト。startDistanceの昇順に並べ、先頭は0(レールの始点)から始める。
	// こうしておくと「区間i = [i番目の開始, i+1番目の開始)」と隣同士だけで範囲が決まり、隙間も重なりも生まれない。
	// 空なら、ステージ全体を1つの区間として扱う(sectionsの無い旧stage.jsonと同じ)
	std::vector<SectionData> sections;

	// レール制御点(CatmullRomSpline用)。ゲーム座標系ネイティブ。空=レールなし
	std::vector<Vector3> rail;

	// カメラ調整値(省略可)。hasCamera=falseならstage.jsonに未記載で、
	// シーンはApplyせずライブ値を保つ(「ファイルに無ければ何もしない」の流儀)
	bool hasCamera = false;
	CameraData camera;

	// ステージ全体の光(省略可)。hasLighting=false(lighting項目の無い旧stage.json)なら光を一切適用せず、
	// エンジンの初期値(昼の見た目)のままにする(cameraと同じ「ファイルに無ければ何もしない」の流儀)
	bool hasLighting = false;
	LightingData lighting;
};
