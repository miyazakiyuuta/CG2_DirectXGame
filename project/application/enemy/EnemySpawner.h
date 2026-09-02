#pragma once
#include "math/Transform.h"
#include "math/Vector3.h"

#include <memory>
#include <string>
#include <vector>

class BulletManager;
class Camera;
class Object3d;
struct SoundData;
struct StageData;

// stage.jsonのSpawnPoint(type="spawn")から敵を発生させる担当。
// 「レール進行度がSpawnPointのrailDistanceを超えたら、その位置に種別に応じた敵を出す」。
// 発生した敵はHPを持ち、自弾が当たるとHPが減り、0になると消える。
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
	/// 進行度を受け取り、到達したSpawnPointの敵を発生させる。
	/// 発生済みの敵の移動・被弾判定・撃破もここで行う
	/// </summary>
	/// <param name="railDistance">プレイヤーのレール上の距離[m]</param>
	/// <param name="deltaTime">経過時間[秒]</param>
	/// <param name="playerPosition">自機のワールド位置(敵の接近先)</param>
	/// <param name="playerRadius">自機の判定半径[m](体当たり判定に使う)</param>
	void Update(float railDistance, float deltaTime, const Vector3& playerPosition, float playerRadius);

	// 発生済みの敵をすべて消して未発生に戻す(レール一周・Resetボタン用)
	void Reset();

	void Draw();

	// 描画に使うカメラを差し替える(生成済みの敵にも反映)
	void SetCamera(Camera* camera);

	/// <summary>
	/// 被弾判定に使う自弾の一覧の取得元。シーンが所有する実体を指すだけ(所有しない)。
	/// nullptrなら被弾判定を行わず、敵は撃破されない
	/// </summary>
	void SetBulletManager(BulletManager* bulletManager) { bulletManager_ = bulletManager; }

	// "Enemy"ウィンドウにHP・移動・判定半径のパラメータを表示する(Debug構成のみ)
	void DrawImGui();

	/// <summary>
	/// 撃破時に発生させるパーティクルのグループ名。
	/// 登録(RegisterEffect)はシーン側、発生はここ、と担当が分かれるので名前をここに集約する
	/// (文字列を両方に直書きすると片方だけ直したときに黙って出なくなる)
	/// </summary>
	static const std::string& DefeatEffectName();

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

	/// <summary>
	/// 今フレーム、自機に接触している敵がいるか(A-5の体当たりダメージ用)。
	/// ダメージの適用そのものは行わない: 無敵時間の管理はPlayerが持つため、
	/// ここは「触れているか」だけを答え、TakeDamageを呼ぶのはシーンの役割にする
	/// </summary>
	bool IsPlayerHit() const { return playerHit_; }

	const std::vector<SpawnPoint>& GetSpawnPoints() const { return spawnPoints_; }
	size_t GetSpawnedCount() const { return enemies_.size(); }
	// 撃破した通算数(ImGui表示用)
	int GetDefeatedCount() const { return defeatedCount_; }

	EnemySpawner();
	~EnemySpawner();

private:
	// 発生済みの敵1体分。見た目(Object3d)と当たり判定に要る状態をまとめて持つ。
	// 弾(Bullet)と違いプールしないのは、敵は同時存在数が少なく、撃破で確実に減るため
	struct Enemy {
		std::unique_ptr<Object3d> object; // 見た目
		Vector3 position{};               // ワールド位置(objectのtranslateと同期させる)
		float radius = 2.5f;              // 当たり判定の半径
		int hp = 3;                       // 残りHP。0で撃破
	};

	// 発生済みの敵を即deleteせず墓場へ移す。ImGuiのボタン処理(Draw後)から呼ばれると、
	// このフレームの描画コマンドが敵の定数バッファを参照済みのため、
	// 即deleteするとOBJECT_DELETED_WHILE_STILL_IN_USEでGPU実行が壊れる(Stageと同じ対策)
	void RetireEnemies();

	// 自弾との当たり判定。命中した弾を消し、敵のHPを減らす
	void UpdateBulletCollision();

	// 自機との接触判定(A-5)。結果をplayerHit_へ書くだけでダメージは与えない
	void UpdatePlayerCollision(const Vector3& playerPosition, float playerRadius);

	// HPの残量を色で表す(満タン=白 → 残り少ない=赤)。
	// なぜ色か: 「敵にHPがある」ことをプレイ動画から読み取れるようにするため。
	// 1発で消えるとHPの存在が画面上に一切現れない
	void ApplyHpColor(Enemy& enemy) const;

	std::vector<SpawnPoint> spawnPoints_;
	// 発生済みの敵の実体
	std::vector<Enemy> enemies_;
	// 破棄待ちの見た目。次のUpdate冒頭(前フレームのGPU実行完了後)にまとめて破棄する
	std::vector<std::unique_ptr<Object3d>> graveyard_;

	// 自弾の一覧の取得元(シーンが所有。所有しない)
	BulletManager* bulletManager_ = nullptr;
	Camera* camera_ = nullptr;

	// 撃破音。SoundManagerのキャッシュと共有し、再生中のPCMを有効に保つ
	std::shared_ptr<const SoundData> defeatSound_;

	// --- 全個体共通のパラメータ(ImGuiで調整する) ---
	int maxHp_ = 3;             // 発生時のHP。1にすると1発で消えるため既定は3
	float radius_ = 2.5f;       // 当たり判定の半径[m](モデルの見た目に合わせる)
	float moveSpeed_ = 3.0f;    // 自機へ近づく速さ[m/s]
	float keepDistance_ = 2.0f; // これ以上は近づかない距離[m]。近づき続けると自機を
	                            // 追い越して背後へ消え、撃つ機会が無くなるため。
	                            // ただし判定半径の和(自機0.5+敵2.5=3.0m)より小さくしないと
	                            // 体当たりが成立しないので、その内側で止まる値にしている
	int defeatParticleCount_ = 40; // 撃破時に出すパーティクルの粒数(見た目の調整用)

	// 今フレーム、自機に接触している敵がいるか(A-5)
	bool playerHit_ = false;

	// 撃破した通算数(ImGui表示用)
	int defeatedCount_ = 0;
};
