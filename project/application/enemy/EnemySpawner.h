#pragma once
#include "math/Transform.h"
#include "math/Vector3.h"

#include <cstdint>
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
	/// <param name="playerForward">自機の進行方向(単位ベクトル)。射撃型が背後から撃たないための向き</param>
	void Update(float railDistance, float deltaTime, const Vector3& playerPosition, float playerRadius,
		const Vector3& playerForward);

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

	/// <summary>
	/// 敵弾の発射先。自弾とは別インスタンスで、シーンが所有する(所有しない)。
	/// nullptrなら敵は撃ってこない(体当たりだけになる)。
	/// 自機に当たったかの判定はPlayer側が行う: 無敵時間の所有者がPlayerなので、
	/// 「弾の一覧を読むのは撃たれる側」という向き(自弾→敵と同じ形)に揃えてある
	/// </summary>
	void SetEnemyBulletManager(BulletManager* bulletManager) { enemyBulletManager_ = bulletManager; }

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

	/// <summary>
	/// 敵の種別ごとの性能。接近型と射撃型の違いは派生クラスを分けずにこの値の差で表す
	/// (ParticleConfigと同じパラメータ駆動。種別を足してもUpdate側の分岐は増えない)。
	/// 種別を増やすときは EnemyTypes() / ModelPathOf() / コンストラクタの既定値 に1件ずつ足す
	/// </summary>
	struct TypeParams {
		std::string name;        // 種別名(stage.jsonのenemyと一致させる)
		int maxHp = 3;           // 発生時のHP。1にすると1発で消えるため既定は3
		float radius = 2.5f;     // 当たり判定の半径[m](モデルの見た目に合わせる)
		float moveSpeed = 3.0f;  // 自機へ近づく速さ[m/s]
		// これ以上は近づかない距離[m]。射撃型はここで止まって撃ち、
		// 接近型は判定半径の和(自機0.5+敵radius)より内側にして体当たりを成立させる
		float keepDistance = 2.0f;
		bool shoot = false;         // 弾を撃つか(falseなら体当たり専門)
		float shootInterval = 2.2f; // 1体あたりの発射間隔[秒]
		// この距離より遠い敵は撃たない[m]。遠すぎる敵に撃たれると、
		// 弾が飛んでくる方向が画面から読み取れないまま被弾する
		float shootRange = 90.0f;
		// 自機にこの距離まで近づいたら接近音を1回鳴らす[m]。0なら鳴らさない。
		// 体当たりの前触れを音で知らせ、視界の端から突っ込んでくる敵にも身構えられるようにする
		float approachSoundRange = 0.0f;
	};

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

	/// <summary>
	/// 狙点が生存中の敵の判定球に入っているか(レティクルの色替え用)。
	/// 敵の位置と半径を知っているのはここだけなので判定もここに置き、
	/// 「どう見せるか」はシーンとReticleに任せる
	/// </summary>
	/// <param name="point">狙点のワールド座標(Player::GetAimPoint)</param>
	bool IsAimedAt(const Vector3& point) const;

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
		// 種別(typeParams_の添字)。半径や速さを写しではなく添字で持つのは、
		// ImGuiでの調整を発生済みの敵にも即座に効かせるため(難易度は走らせながら詰める)
		size_t typeIndex = 0;
		int hp = 3;                       // 残りHP。0で撃破(最大値は種別のmaxHp)
		// 次に撃つまでの残り時間[秒]。個体ごとに持たせるのは、
		// 共通タイマーだと同時に出た敵が必ず同時に撃ち、避けようのない斉射になるため
		float shootCooldown = 0.0f;
		// 接近音を鳴らし済みか(1体につき1回だけ。近くに居続けても鳴り直さない)
		bool approachAlerted = false;
	};

	// 発生済みの敵を即deleteせず墓場へ移す。ImGuiのボタン処理(Draw後)から呼ばれると、
	// このフレームの描画コマンドが敵の定数バッファを参照済みのため、
	// 即deleteするとOBJECT_DELETED_WHILE_STILL_IN_USEでGPU実行が壊れる(Stageと同じ対策)
	void RetireEnemies();

	// 自弾との当たり判定。命中した弾を消し、敵のHPを減らす
	void UpdateBulletCollision();

	// 自機との接触判定(A-5)。結果をplayerHit_へ書くだけでダメージは与えない
	void UpdatePlayerCollision(const Vector3& playerPosition, float playerRadius);

	// 各個体のクールタイムを消化し、来た敵から自機へ向けて撃つ。
	// 移動後に呼ぶこと(撃つ瞬間の位置から弾を出すため)
	void UpdateShooting(float deltaTime, const Vector3& playerPosition, const Vector3& playerForward);

	// 種別のapproachSoundRangeまで近づいた敵の接近音を鳴らす。
	// 移動後に呼ぶこと(近づいた瞬間の距離で判定するため)
	void UpdateApproachSound(const Vector3& playerPosition);

	// 次のクールタイムを引く。個体差を付けて斉射を防ぐ(発生時の初回待ちにも使う)
	float NextShootCooldown(const TypeParams& params) const;

	// 種別名 → typeParams_の添字。未知の種別は先頭(既定)へ落とす(ModelPathOfと同じ作法)
	size_t TypeIndexOf(const std::string& enemy) const;
	// 個体の性能を引く。添字が範囲外でも落ちないよう丸める
	const TypeParams& ParamsOf(const Enemy& enemy) const;

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
	// 敵弾の発射先(シーンが所有。所有しない)
	BulletManager* enemyBulletManager_ = nullptr;
	Camera* camera_ = nullptr;

	// 撃破音。SoundManagerのキャッシュと共有し、再生中のPCMを有効に保つ
	std::shared_ptr<const SoundData> defeatSound_;
	// 接近音(撃破音と同じ持ち方)
	std::shared_ptr<const SoundData> approachSound_;
	// 最後に鳴らした接近音のSoundManager::SoundHandle。0は未再生(InvalidHandle)。
	// ヘッダでSoundManager.h(XAudio2)を読まないよう、実体の型(uint32_t)で持つ
	uint32_t approachSoundHandle_ = 0;

	// --- 種別ごとのパラメータ(ImGuiで調整する) ---
	// 既定値はコンストラクタで入れる。並びはEnemyTypes()と揃える
	std::vector<TypeParams> typeParams_;

	// --- 全種別で共通のパラメータ ---
	int defeatParticleCount_ = 40; // 撃破時に出すパーティクルの粒数(見た目の調整用)
	bool shootEnabled_ = true;     // 撃ってくるか(難易度確認とデバッグ用に丸ごと止められる)
	// 間隔に掛ける乱数の幅(±割合)。0なら同じ種別の敵が全員同じ周期で撃つ
	float shootIntervalRandom_ = 0.35f;
	int shotCount_ = 0; // 通算の発射数(ImGui表示用)

	// 今フレーム、自機に接触している敵がいるか(A-5)
	bool playerHit_ = false;

	// 撃破した通算数(ImGui表示用)
	int defeatedCount_ = 0;
};
