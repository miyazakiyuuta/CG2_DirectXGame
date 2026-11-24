#pragma once
#include "enemy/EnemyTypeParams.h"
#include "math/Transform.h"
#include "math/Vector3.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BulletManager;
class Camera;
class Enemy;
class Object3d;
struct SoundData;
struct StageData;

// stage.jsonのSpawnPoint(type="spawn")から敵を発生させる担当。
// 「レール進行度がSpawnPointのrailDistanceを超えたら、その位置に種別に応じた敵を出す」。
// 発生した敵はHPを持ち、自弾が当たるとHPが減り、0になると消える。
// 敵の実体は基底クラスEnemyのポインタで持ち、全種をまとめて回す。種別ごとに違う「どう動き、どう攻めるか」は
// Enemyの派生クラス(射撃型ShooterEnemy・体当たり型RammerEnemy)がUpdateBehaviorで受け持ち、
// ここは全種で共通の処理(発生・弾との判定・爆発・通過・全滅・ロックオンの問い合わせ)だけを持つ。
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

	// 戦闘中の敵の当たり判定の球をデバッグ表示に積む(Show Colliders用。描くのはDebugRenderer)
	void DrawHitSpheres() const;

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
	// (敵の実体が無くても引けるよう、種別名から引く静的な表にしておく。クリア演出・エディタも使う)
	static const std::string& ModelPathOf(const std::string& enemy);
	// 種別に応じた表示スケール(モデルごとに実寸が違うため)
	static float ScaleOf(const std::string& enemy);
	// エディタのCombo用。ここに足せばUIとゲーム側の対応が同時に増える
	static const std::vector<std::string>& EnemyTypes();

	/// <summary>
	/// ステージに「倒すとクリアになる敵」(isGoalの種別。女王)の出現地点があるか。
	/// あればクリア条件は「終点到達」ではなく「それを倒す」になる
	/// </summary>
	bool HasGoal() const;

	/// <summary>
	/// 「倒すとクリアになる敵」をすべて倒したか。HasGoalがfalseなら常にfalse。
	/// 撃破の記録(SpawnPoint::outcome)を見るだけなので、Reset・BuildFromStageで自動的にfalseへ戻る
	/// </summary>
	bool IsGoalDefeated() const;

	/// <summary>
	/// 倒すとクリアになる敵(女王)の姿。クリア演出が、同じ場所・姿勢・大きさに亡骸を置き直すのに使う
	/// </summary>
	struct GoalPose {
		std::string enemy;   // 種別名(ModelPathOfでモデルを引く)
		Transform transform; // 見た目の位置・回転・大きさ(亡骸を置く)
		Vector3 center;      // 当たり判定の球の中心(体の前寄り。小爆発と大爆発はここを中心にする)
		float radius = 0.0f; // 判定半径(小爆発を散らす範囲と、爆発の大きさの基準)
	};

	/// <summary>
	/// 女王が倒れた瞬間の姿を引く。撃破の処理(Update)の中で記録したもの
	/// </summary>
	/// <returns>まだ倒していなければ(Reset・BuildFromStageの後も)false</returns>
	bool TryGetGoalDefeatPose(GoalPose& outPose) const;

	/// <summary>
	/// 女王の出現地点の姿を引く。Edit中にクリア演出だけを再生して見た目を調整する(Preview Clear)のに使う
	/// </summary>
	/// <returns>ステージに女王がいなければfalse</returns>
	bool TryGetGoalSpawnPose(GoalPose& outPose) const;

	/// <summary>
	/// 生存中の女王を倒したことにする(ImGuiのForce Clear用)。HPを0にするだけで、
	/// 撃破の処理(爆発・撃破音・記録)は次のUpdateで普段の撃破と同じ流れを通る
	/// </summary>
	/// <returns>生存中の女王がいたか(まだ出ていなければfalse)</returns>
	bool ForceDefeatGoal();

	/// <summary>
	/// 女王が倒れた瞬間(クリア演出の始まり)。残りの敵をその場で固め、動き・攻撃・接触・被弾をやめさせる。
	/// 新しい敵も出さない。Reset・BuildFromStageで元に戻る
	/// </summary>
	void FreezeAll();

	/// <summary>
	/// 大爆発の瞬間(クリア演出)。残りの敵が色を失って落ち、時間差で煙になって消える(巣の全滅)。
	/// 撃破にも取り逃しにも数えない: 評価(駆除数・ランク)はプレイヤーが自分で倒した分だけにするため
	/// </summary>
	void ExterminateAll();

	/// <summary>
	/// 全滅で消えるときの煙のパーティクルのグループ名。DefeatEffectNameと同じく、
	/// 登録(RegisterEffect)はシーン、発生はここ、と担当が分かれるので名前をここに集約する
	/// </summary>
	static const std::string& ExterminateEffectName();

	// SpawnPoint1件分(データの写し。StageDataを構造変更しても参照が壊れないよう値で持つ)
	struct SpawnPoint {
		// ここから出た敵の結末。区間ごとの集計(StageProgress)がこれを数える
		enum class Outcome {
			Pending,  // まだ決まっていない(未発生・交戦中)
			Defeated, // 倒した
			Missed,   // 取り逃した(自機の後ろへ抜けた。通過済みの敵はもう撃ち落とせないので、抜けた時点で確定)
		};

		std::string enemy;
		float railDistance = 0.0f;
		Transform transform;
		bool spawned = false; // 発生済みか(進行度を戻すまで再発生しない)
		Outcome outcome = Outcome::Pending;
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
	/// <summary>
	/// 種別名から、その振る舞いを持つ派生クラスの敵を作る(SceneFactoryと同じ「名前→実体」の窓口)。
	/// 呼び出し側は基底のポインタ(Enemy)で受け取るので、どの派生クラスかを知らずに済む。
	/// 未知の種別は既定(射撃型)へ落とす(ModelPathOfと同じ扱い。データ不備でも敵が消えないので気付ける)
	/// </summary>
	static std::unique_ptr<Enemy> CreateEnemy(const std::string& enemy);

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

	/// <summary>
	/// 敵の結末(撃破・取り逃し)を、その敵が出たSpawnPointへ書き残す。
	/// 区間の集計(StageProgress)はこの記録を数えるだけなので、EnemySpawnerは区間を知らなくてよい
	/// </summary>
	void RecordOutcome(const Enemy& enemy, SpawnPoint::Outcome outcome);

	// 番号から生存中の敵を探す。見つからなければnullptr。
	// 返したポインタは、その敵が一覧から取り除かれるまで(撃破・離脱・Resetまで)しか使わないこと
	const Enemy* FindEnemyById(uint32_t enemyId) const;

	// 自機との接触判定(A-5)。結果をplayerHit_へ書くだけでダメージは与えない
	void UpdatePlayerCollision(const Vector3& playerPosition, float playerRadius);

	// 接近音を鳴らす。同時に鳴らすのは1つまで(敵は2〜3体まとめて出るので、重ねると音が割れて
	// 「近づいてきた」という合図として聞き取れなくなる。1つ鳴っていれば合図としては足りる)。
	// 「どの敵が間合いに入ったか」は体当たり型(RammerEnemy)が判定して報告し、鳴らすかどうかだけをここで決める
	void PlayApproachSound();

	/// <summary>
	/// 自機より後ろへ回った敵を通過済みにし、画面の外側へ離脱させる。離脱し終えた敵は消す(撃破扱いにしない)。
	/// 追尾より先に呼ぶこと(通過済みになったフレームから追わせないため)
	/// </summary>
	/// <param name="deltaTime">経過時間[秒]</param>
	/// <param name="playerPosition">自機のワールド位置</param>
	/// <param name="playerForward">自機の進行方向(レールの接線。単位ベクトル)</param>
	void UpdateEscape(float deltaTime, const Vector3& playerPosition, const Vector3& playerForward);

	/// <summary>
	/// 固まっている間(FreezeAll以降)の更新。全滅で落ちている敵を落とし、落ち切った敵を煙にして消す。
	/// 固まっているだけの敵は動かさない(見た目の行列だけ更新する)
	/// </summary>
	void UpdateFrozen(float deltaTime);

	// 種別名 → typeParams_の添字。未知の種別は先頭(既定)へ落とす(ModelPathOfと同じ作法)
	size_t TypeIndexOf(const std::string& enemy) const;
	// 個体の性能を引く。添字が範囲外でも落ちないよう丸める
	const EnemyTypeParams& ParamsOf(const Enemy& enemy) const;

	// 当たり判定の球の中心(種別のhitOffsetだけ頭の方へずらした点。ずらさない種別は位置そのもの)。
	// 位置(position)は動かす計算に、この中心は当てる・狙う・撃つ計算に使う
	Vector3 HitCenter(const Enemy& enemy) const;

	std::vector<SpawnPoint> spawnPoints_;
	// 発生済みの敵の実体。基底のポインタで持ち、種別(派生クラス)を問わず同じ一覧で回す。
	// 値(vector<Enemy>)ではなくポインタで持つのは、派生クラスごとに大きさが違い、値で持つと派生部分が切り落とされるため
	// (スライシング)。要素がヒープ上で動かないので、FindEnemyByIdが返すポインタも一覧の詰め直しで無効にならない
	std::vector<std::unique_ptr<Enemy>> enemies_;
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
	std::vector<EnemyTypeParams> typeParams_;

	// --- 全種別で共通のパラメータ ---
	int defeatParticleCount_ = 40; // 撃破時に出すパーティクルの粒数(見た目の調整用)
	// 敵の攻撃の切り替え(難易度確認とデバッグ用に丸ごと止められる)
	bool shootEnabled_ = true; // 撃ってくるか
	// 体当たりしてくるか。OFFの間は、体当たり型(RammerEnemy)がその場に留まり、
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

	// --- 巣の全滅(クリア演出) ---
	// 固まっているか(FreezeAll以降)。立っている間は、発生・被弾・移動・攻撃・接触を行わない
	bool frozen_ = false;
	// 女王が倒れた瞬間の姿(TryGetGoalDefeatPose)
	bool hasGoalDefeatPose_ = false;
	GoalPose goalDefeatPose_;
	// 落ちる重力[m/s^2]。床まで約1秒で届く強さ(現実の9.8より強くして、ポトッと落ちる手応えにする)
	float exterminateGravity_ = 30.0f;
	// 落ち始めてから消えるまで[秒]の範囲。個体ごとにこの間で散らす
	float exterminateLifeMin_ = 0.5f;
	float exterminateLifeMax_ = 1.4f;
	int exterminateSmokeCount_ = 12; // 消えるときの煙の粒の数

	// 今フレーム、自機に接触している敵がいるか(A-5)と、その敵の位置
	bool playerHit_ = false;
	Vector3 playerHitPosition_ = { 0.0f, 0.0f, 0.0f };

	// 撃破した通算数(ImGui表示用)
	int defeatedCount_ = 0;
	// 爆発した通算回数(ImGui表示用)
	int explosionCount_ = 0;
};
