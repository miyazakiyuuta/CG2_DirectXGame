#pragma once
#include "math/Transform.h"

#include <memory>
#include <string>
#include <vector>

class Camera;
class Object3d;
struct StageData;

// stage.jsonのSpawnPoint(type="spawn")から敵を発生させる担当。
// 「レール進行度がSpawnPointのrailDistanceを超えたら、その位置に種別に応じた敵を出す」。
// なぜStageではなくここに置くか: 敵と進行度はゲーム固有の概念で、Stageは
// 「ステージデータからランタイム実体を作る器」という役割に留めたいため
// (application/stage/にゲーム進行のロジックを持ち込まない)。
class EnemySpawner {
public:
	/// <summary>
	/// StageDataのSpawnPointを取り込み、未発生状態にする。
	/// ステージのReload/Rebuild後や、シーン初期化時に呼ぶ
	/// </summary>
	void BuildFromStage(const StageData& stageData);

	/// <summary>
	/// 進行度を受け取り、到達したSpawnPointの敵を発生させる
	/// </summary>
	/// <param name="railDistance">プレイヤーのレール上の距離[m]</param>
	void Update(float railDistance, float deltaTime);

	// 発生済みの敵をすべて消して未発生に戻す(レール一周・Resetボタン用)
	void Reset();

	void Draw();

	// 描画に使うカメラを差し替える(生成済みの敵にも反映)
	void SetCamera(Camera* camera);

	// 敵の種別に対応するモデルパス。未知の種別は既定モデルを返す
	static const std::string& ModelPathOf(const std::string& enemy);
	// 種別に応じた表示スケール(モデルごとに実寸が違うため)
	static float ScaleOf(const std::string& enemy);
	// エディタのCombo用。ここに足せばUIとゲーム側の対応が同時に増える
	static const std::vector<std::string>& EnemyTypes();

	// SpawnPoint1件分(データの写し。StageDataを構造変更しても参照が壊れないよう値で持つ)
	struct SpawnPoint {
		std::string enemy;
		float railDistance = 0.0f;
		Transform transform;
		bool spawned = false; // 発生済みか(進行度を戻すまで再発生しない)
	};

	const std::vector<SpawnPoint>& GetSpawnPoints() const { return spawnPoints_; }
	size_t GetSpawnedCount() const { return enemies_.size(); }

	EnemySpawner();
	~EnemySpawner();

private:
	// 発生済みの敵を即deleteせず墓場へ移す。ImGuiのボタン処理(Draw後)から呼ばれると、
	// このフレームの描画コマンドが敵の定数バッファを参照済みのため、
	// 即deleteするとOBJECT_DELETED_WHILE_STILL_IN_USEでGPU実行が壊れる(Stageと同じ対策)
	void RetireEnemies();

	std::vector<SpawnPoint> spawnPoints_;
	// 発生済みの敵の実体(見た目のみ。移動やHPは未実装)
	std::vector<std::unique_ptr<Object3d>> enemies_;
	// 破棄待ちの敵。次のUpdate冒頭(前フレームのGPU実行完了後)にまとめて破棄する
	std::vector<std::unique_ptr<Object3d>> graveyard_;
	Camera* camera_ = nullptr;
};
