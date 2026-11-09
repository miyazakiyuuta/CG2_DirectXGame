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
	/// 被弾判定に使うチャージ弾の一覧の取得元。シーンが所有する実体を指すだけ(所有しない)。
	/// 通常弾と同じ判定を通し、違い(ダメージ・爆発の半径)は弾のProfileから読む。
	/// nullptrならチャージ弾では被弾しない
	/// </summary>
	void SetChargeBulletManager(BulletManager* bulletManager) { chargeBulletManager_ = bulletManager; }

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
	// 今フレーム自機に接触している敵の位置(IsPlayerHitがtrueのときだけ意味を持つ)。
	// シーンがTakeDamageへ「どこから当たったか」として渡す
	const Vector3& GetPlayerHitPosition() const { return playerHitPosition_; }

	/// <summary>
	/// 狙点が生存中の敵の判定球に入っているか(レティクルの色替え用)。
	/// 敵の位置と半径を知っているのはここだけなので判定もここに置き、
	/// 「どう見せるか」はシーンとReticleに任せる
	/// </summary>
	/// <param name="point">狙点のワールド座標(Player::GetAimPoint)</param>
	bool IsAimedAt(const Vector3& point) const;

	/// <summary>
	/// 視線(originからdirection方向へまっすぐ伸ばした線=レイ)のまわりの円錐に入っている生存中の敵のうち、
	/// 視線に一番近い(視線からの角度が一番小さい)敵の番号を返す。ロックオンの吸い付き先を探すのに使う。
	/// originにカメラ位置、directionにカメラ→狙点の向きを渡すと、角度はそのまま
	/// 「画面上でレティクルからどれだけ離れて見えるか」になる(カメラを通る直線は画面上の1点に重なるため)
	/// </summary>
	/// <param name="origin">視線の始点(カメラ位置)</param>
	/// <param name="direction">視線の向き(単位ベクトル)</param>
	/// <param name="maxAngle">円錐の半角[rad]。敵の見た目の縁がこの角度以内なら対象にする。
	/// 0なら「レティクルが敵に重なっている」ときだけ、大きくするほど離れた敵にも吸い付く</param>
	/// <param name="maxDistance">これより遠い敵は対象にしない[m]</param>
	/// <returns>敵の番号。見つからなければ0</returns>
	uint32_t FindEnemyInCone(const Vector3& origin, const Vector3& direction, float maxAngle, float maxDistance) const;

	/// <summary>
	/// 番号から生存中の敵の位置を引く。ロックした敵の追跡と、チャージ弾の追尾先に使う。
	/// 添字ではなく番号で引くのは、敵の一覧が撃破のたびに詰められ、並び順が変わってしまうため
	/// </summary>
	/// <param name="enemyId">敵の番号(FindEnemyInConeの戻り値)</param>
	/// <param name="outPosition">見つかった敵のワールド位置</param>
	/// <returns>居なければ(撃破済み・通過済み・Reset済み・0を渡した)false</returns>
	bool TryGetEnemyPosition(uint32_t enemyId, Vector3& outPosition) const;

	/// <summary>
	/// 番号から生存中の敵の位置と判定半径を引く。ロック枠を敵の大きさに合わせて描くのに使う
	/// </summary>
	/// <returns>居なければfalse(TryGetEnemyPositionと同じ)</returns>
	bool TryGetEnemySphere(uint32_t enemyId, Vector3& outCenter, float& outRadius) const;

	/// <summary>
	/// 爆発した通算回数。シーンが更新の前後で比べ、増えていたら手応えの演出(画面の揺れ・ヒットストップ)を出す
	/// (Playerの被弾をHPの増減で拾うのと同じ、「結果の差分を見る」やり方)
	/// </summary>
	int GetExplosionCount() const { return explosionCount_; }

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
		// 出現時に振る番号(1から。0は「なし」を表すので振らない)。ロックオンと追尾が敵を指すのに使う
		uint32_t id = 0;
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
		// 自機を追う速度[m/s](向き×種別の速さ。止まっている間は0)。
		// 通過済みになった瞬間に離脱の初速として引き継ぎ、向きが急に変わって見えないようにする
		Vector3 velocity{};
		// 通過済みか(自機より後ろへ回り、攻撃をやめて離脱している)。一度立ったら戻らない:
		// 画面外から追いついての体当たりを防ぐのが目的なので、後ろへ回った時点で戦闘から降ろす
		bool passed = false;
		// 離脱する向き(単位ベクトル)。通過した瞬間に決めて以後変えない
		Vector3 escapeDirection{};
		// 通過済みになってからの経過時間[秒]。escapeLifetime_を超えたら消す
		float escapeTime = 0.0f;
	};

	// 発生済みの敵を即deleteせず墓場へ移す。ImGuiのボタン処理(Draw後)から呼ばれると、
	// このフレームの描画コマンドが敵の定数バッファを参照済みのため、
	// 即deleteするとOBJECT_DELETED_WHILE_STILL_IN_USEでGPU実行が壊れる(Stageと同じ対策)
	void RetireEnemies();

	// 自弾との当たり判定。命中した弾を消し、敵のHPを減らす。
	// 爆発する弾(explosionRadius>0)は、当たった位置の周りの敵もまとめて巻き込む。
	// 通常弾とチャージ弾の両方にこの関数を使う(違いは弾のProfileの値だけなので、型で分けない)
	void UpdateBulletCollision(BulletManager* bulletManager);

	/// <summary>
	/// 爆発。爆心から半径内(敵の判定球と重なる範囲)にいる生存中の敵すべてにダメージを与える。
	/// 撃破の処理(演出・削除・撃破数)はUpdateの既存の流れに任せる。
	/// 爆発そのものの見た目と音は、弾が消えるときの演出としてBulletManagerが出す(ここはダメージだけ)
	/// </summary>
	/// <param name="center">爆心(弾が当たった位置)</param>
	/// <param name="radius">爆発の半径[m]</param>
	/// <param name="damage">巻き込んだ敵それぞれに与えるダメージ</param>
	/// <param name="directHit">弾が直撃した敵。半径に関係なく必ず巻き込む</param>
	void Explode(const Vector3& center, float radius, int damage, const Enemy& directHit);

	// HPを減らし、残っていれば色に反映する(撃破されたら消えるので色は変えない)
	void ApplyDamage(Enemy& enemy, int damage) const;

	// 番号から生存中の敵を探す。見つからなければnullptr。
	// 返したポインタは敵の一覧を変えるまで(次の発生・撃破まで)しか使わないこと(vectorの要素を指すため)
	const Enemy* FindEnemyById(uint32_t enemyId) const;

	// 自機との接触判定(A-5)。結果をplayerHit_へ書くだけでダメージは与えない
	void UpdatePlayerCollision(const Vector3& playerPosition, float playerRadius);

	// 各個体のクールタイムを消化し、来た敵から自機へ向けて撃つ。
	// 移動後に呼ぶこと(撃つ瞬間の位置から弾を出すため)
	void UpdateShooting(float deltaTime, const Vector3& playerPosition, const Vector3& playerForward);

	// 種別のapproachSoundRangeまで近づいた敵の接近音を鳴らす。
	// 移動後に呼ぶこと(近づいた瞬間の距離で判定するため)
	void UpdateApproachSound(const Vector3& playerPosition);

	/// <summary>
	/// 自機より後ろへ回った敵を通過済みにし、画面の外側へ離脱させる。離脱し終えた敵は消す(撃破扱いにしない)。
	/// 追尾より先に呼ぶこと(通過済みになったフレームから追わせないため)
	/// </summary>
	/// <param name="deltaTime">経過時間[秒]</param>
	/// <param name="playerPosition">自機のワールド位置</param>
	/// <param name="playerForward">自機の進行方向(レールの接線。単位ベクトル)</param>
	void UpdateEscape(float deltaTime, const Vector3& playerPosition, const Vector3& playerForward);

	// 戦闘に参加している敵か(生存中かつ通過済みでない)。
	// 被弾・ロックオン・照準の対象はこれで絞る。通過済みの敵は見た目だけ残って去っていく
	static bool IsInCombat(const Enemy& enemy) { return enemy.hp > 0 && !enemy.passed; }

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
	// 次に出現する敵へ振る番号。Resetでも戻さない: 番号を使い回すと、
	// 飛んでいるチャージ弾やロックが持っている古い番号が、新しく出た別の敵を指してしまうため
	uint32_t nextEnemyId_ = 1;
	// 破棄待ちの見た目。次のUpdate冒頭(前フレームのGPU実行完了後)にまとめて破棄する
	std::vector<std::unique_ptr<Object3d>> graveyard_;

	// 自弾の一覧の取得元(シーンが所有。所有しない)
	BulletManager* bulletManager_ = nullptr;
	// チャージ弾の一覧の取得元(シーンが所有。所有しない)
	BulletManager* chargeBulletManager_ = nullptr;
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
	// 敵の攻撃の切り替え(難易度確認とデバッグ用に丸ごと止められる)
	bool shootEnabled_ = true; // 撃ってくるか
	// 体当たりしてくるか。OFFの間は、接近型(撃たない種別)がその場に留まり、
	// どの種別に触れてもダメージにならない(接近の予告音も鳴らさない)
	bool ramEnabled_ = true;
	// 間隔に掛ける乱数の幅(±割合)。0なら同じ種別の敵が全員同じ周期で撃つ
	float shootIntervalRandom_ = 0.35f;
	int shotCount_ = 0; // 通算の発射数(ImGui表示用)

	// --- 通過済みの敵の離脱 ---
	// 自機よりこれだけ後ろへ回ったら通過済みにする[m]。通過済みは戻らないのでちらつきは起きず、
	// この値は「自機の真横〜少し後ろなら、まだ体当たりしてきてよい」範囲を決める
	// (体当たりが届くのは判定半径の和以内=自機のすぐそばなので、この範囲の敵は画面に映っている)
	float passMargin_ = 2.0f;
	// 画面の外側へ抜けていく速さ[m/s]
	float escapeSpeed_ = 8.0f;
	// 追っていた勢いを落とすブレーキ[m/s^2]。9m/sの接近型なら約1.1秒で止まる
	float escapeBrake_ = 8.0f;
	// 通過済みになってから消えるまで[秒]。レール速度8m/sなら3秒でカメラ(自機の12m後ろ)より後ろへ抜けている
	float escapeLifetime_ = 3.0f;

	// 今フレーム、自機に接触している敵がいるか(A-5)と、その敵の位置
	bool playerHit_ = false;
	Vector3 playerHitPosition_ = { 0.0f, 0.0f, 0.0f };

	// 撃破した通算数(ImGui表示用)
	int defeatedCount_ = 0;
	// 爆発した通算回数(ImGui表示用)
	int explosionCount_ = 0;
};
