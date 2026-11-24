#pragma once
#include "enemy/EnemyTypeParams.h"
#include "math/Transform.h"
#include "math/Vector3.h"

#include <cstddef>
#include <cstdint>
#include <memory>

class BulletManager;
class Camera;
class Object3d;
struct Vector4;

/// <summary>
/// 発生済みの敵1体分の基底クラス。
/// 全種で共通の状態と処理(HP・被弾・通過済みの離脱・全滅で落ちる・描画)はここに置き、
/// 種別ごとに違う「どう動き、どう攻めるか」だけを純粋仮想関数UpdateBehaviorとして派生クラスに任せる。
/// EnemySpawnerは基底のポインタ(Enemy*)だけを見て全種をまとめて回すので、種別を足してもSpawnerの更新処理は変わらない。
///
/// 継承の形:
///   Enemy(抽象)
///    ├ RammerEnemy  … 体当たり型(sword)。一気に間合いを詰め、近づいたら接近音で予告する
///    └ ShooterEnemy … 射撃型(frog・queen)。間合いを保って止まり、自機へ撃つ
/// 「撃つ」機能は基底に置かない: 体当たり型は撃たないので、基底に置くと「何もしないShoot」を持たされる
/// (飛べない鳥にFlyを持たせるのと同じ誤り)。撃つ敵の共通処理はShooterEnemyにまとめる。
///
/// 種別の数値(HP・速さ・射程など)は写しを持たず、毎フレームEnemySpawnerから受け取る
/// (ImGuiでの調整を発生済みの敵にも即座に効かせるため。どの数値を使うかはtypeIndexで決まる)
/// </summary>
class Enemy {
public:
	/// <summary>
	/// このフレームの状況。敵はこれを読むだけで、シーンやEnemySpawnerを知らない
	/// </summary>
	struct Context {
		float deltaTime = 0.0f;            // 経過時間[秒](ゲーム時間。スロー・ヒットストップが掛かった後)
		Vector3 playerPosition{};          // 自機のワールド位置(近づく先・狙う先)
		Vector3 playerForward{};           // 自機の進行方向(単位ベクトル)。背後から撃たないための向き
		BulletManager* enemyBulletManager = nullptr; // 敵弾の発射先。nullptrなら撃たない
		bool shootEnabled = true;          // 撃ってよいか(ImGuiで全種まとめて止められる)
		bool ramEnabled = true;            // 体当たりしてよいか(同上)
		float shootIntervalRandom = 0.0f;  // 発射間隔に掛ける乱数の幅(±割合)
	};

	/// <summary>
	/// UpdateBehaviorの結果。敵どうしで共有するもの(接近音の再生・発射数の集計)は敵が自分で触らず、
	/// ここで報告してEnemySpawnerに任せる(接近音は「同時に1つまで」なので、全体を見ている側でしか絞れない)
	/// </summary>
	struct BehaviorResult {
		bool approached = false; // 体当たりの間合いに入った(接近音の合図)
		int shotCount = 0;       // このフレームに撃った数
	};

	// 基底のポインタ(unique_ptr<Enemy>)経由で破棄するので、デストラクタは仮想にする
	// (非仮想だと派生クラスのデストラクタが呼ばれず、派生側のメンバが解放されない)
	virtual ~Enemy();

	/// <summary>
	/// 出現させる。共通の状態を入れたあと、種別ごとの準備(OnSpawn)を呼ぶ
	/// </summary>
	/// <param name="id">出現時に振る番号(1から。0は「なし」)</param>
	/// <param name="spawnIndex">どの出現地点から出たか(結末を書き残す先)</param>
	/// <param name="typeIndex">種別(EnemySpawnerの種別一覧の添字)</param>
	/// <param name="object">見た目。位置・回転・大きさを設定済みのもの</param>
	/// <param name="params">種別の数値(HPの最大値と、OnSpawnでの準備に使う)</param>
	/// <param name="context">出現したフレームの状況</param>
	void Initialize(uint32_t id, size_t spawnIndex, size_t typeIndex, std::unique_ptr<Object3d> object,
		const EnemyTypeParams& params, const Context& context);

	/// <summary>
	/// どう動き、どう攻めるか(種別ごとの違い)。戦闘中の敵(通過済みでない)にだけ毎フレーム呼ばれる。
	/// 見た目の行列の更新(UpdateObject)は呼び出し側がこの後にまとめて行う
	/// </summary>
	virtual BehaviorResult UpdateBehavior(const EnemyTypeParams& params, const Context& context) = 0;

	// --- 被弾・撃破 ---
	// HPを減らし、残っていれば色に反映する(0未満にはしない。撃破されたら消えるので色は変えない)
	void ApplyDamage(int damage, int maxHp);
	// 倒したことにする(ImGuiのForce Clear用)。撃破の処理は次の更新で普段の撃破と同じ流れを通る
	void ForceDefeat() { hp_ = 0; }
	bool IsDefeated() const { return hp_ <= 0; }
	// 戦闘に参加している敵か(生存中かつ通過済みでも全滅で落ちている最中でもない)。
	// 被弾・ロックオン・照準の対象はこれで絞る。通過済みの敵は見た目だけ残って去っていく
	bool IsInCombat() const { return hp_ > 0 && !passed_ && !dying_; }

	// --- 通過済み(自機の後ろへ回った敵の離脱) ---
	// 通過済みにする。一度立ったら戻らない(画面外から追いついての体当たりを防ぐのが目的)
	void StartEscape(const Vector3& direction);
	// 追っていた勢いをブレーキで落としながら、外側へ抜ける速さで離れていく
	void UpdateEscape(float deltaTime, float brake, float speed);
	bool IsPassed() const { return passed_; }
	float GetEscapeTime() const { return escapeTime_; } // 通過済みになってからの経過時間[秒]

	// --- 巣の全滅(クリア演出) ---
	// 色を失って落ち始める(力が抜けて足場を離れる)。戦闘からは降りる
	void StartDying(float duration, const Vector3& velocity, float spin, const Vector4& color);
	// 重力で落とし、転がす
	void UpdateDying(float deltaTime, float gravity);
	bool IsDying() const { return dying_; }
	bool IsDyingFinished() const { return dying_ && dyingTime_ >= dyingDuration_; }

	// --- 見た目 ---
	void UpdateObject(float deltaTime); // 見た目の行列を更新する
	void Draw();
	void SetCamera(Camera* camera);
	/// <summary>
	/// 見た目を手放す(EnemySpawnerが墓場へ移して、GPUの実行が終わってから破棄する)。
	/// 呼んだ後のこの敵は、一覧から取り除く以外に使わないこと
	/// </summary>
	std::unique_ptr<Object3d> ReleaseObject();

	/// <summary>
	/// 当たり判定の球の中心(種別のhitOffsetだけ頭の方へずらした点。ずらさない種別は位置そのもの)。
	/// 位置(GetPosition)は動かす計算に、この中心は当てる・狙う・撃つ計算に使う
	/// </summary>
	Vector3 HitCenter(const EnemyTypeParams& params) const;

	/// <summary>
	/// 位置から、回転したモデルの前(頭の向き。モデルの規格でローカル-Z)へoffsetだけ進めた点
	/// </summary>
	static Vector3 ForwardPoint(const Vector3& position, const Vector3& rotate, float offset);

	uint32_t GetId() const { return id_; }
	size_t GetSpawnIndex() const { return spawnIndex_; }
	size_t GetTypeIndex() const { return typeIndex_; }
	const Vector3& GetPosition() const { return position_; }
	// 今の姿(見た目の回転・大きさと、ワールド位置)。女王が倒れた瞬間の姿を控えるのに使う
	Transform GetPose() const;

protected:
	/// <summary>
	/// 出現した瞬間の、種別ごとの準備。既定では何もしない(射撃型は初回の発射までの待ちを決める)。
	/// 既定を「何もしない」にしてよいのは、「準備が要らない」ことが嘘ではないため
	/// (飛べない鳥のFlyと違い、何もしないこと自体が正しい振る舞いになる)
	/// </summary>
	virtual void OnSpawn(const EnemyTypeParams& params, const Context& context);

	/// <summary>
	/// 自機へ近づく。ただし種別のkeepDistanceより内側には入らない(行き過ぎて反転しないよう、残り距離を超えて進まない)。
	/// 体当たり型も射撃型も「近づき方」は同じで、違うのは数値(速さ・止まる距離)と、近づいた後に何をするかだけ
	/// </summary>
	void Approach(const EnemyTypeParams& params, const Vector3& target, float deltaTime);

	/// <summary>
	/// その場に留まる(速度を0にする)。通過済みになった瞬間に離脱の初速として引き継ぐので、止まっている間は0にしておく
	/// </summary>
	void Hold() { velocity_ = { 0.0f, 0.0f, 0.0f }; }

private:
	// HPの残量を色で表す(満タン=白 → 残り少ない=赤)。
	// なぜ色か: 「敵にHPがある」ことをプレイ動画から読み取れるようにするため。
	// 1発で消えるとHPの存在が画面上に一切現れない
	void ApplyHpColor(int maxHp);

	// 出現時に振る番号(1から。0は「なし」を表すので振らない)。ロックオンと追尾が敵を指すのに使う
	uint32_t id_ = 0;
	// どの出現地点から出たか(EnemySpawnerの出現地点の添字)。結末(撃破・取り逃し)をそこへ書き残すのに使う
	size_t spawnIndex_ = 0;
	// 種別(EnemySpawnerの種別一覧の添字)。数値を写しではなく添字で持つのは、
	// ImGuiでの調整を発生済みの敵にも即座に効かせるため(難易度は走らせながら詰める)
	size_t typeIndex_ = 0;
	std::unique_ptr<Object3d> object_; // 見た目
	Vector3 position_{};               // ワールド位置(object_のtranslateと同期させる)
	int hp_ = 3;                       // 残りHP。0で撃破(最大値は種別のmaxHp)
	// 自機を追う速度[m/s](向き×種別の速さ。止まっている間は0)。
	// 通過済みになった瞬間に離脱の初速として引き継ぎ、向きが急に変わって見えないようにする
	Vector3 velocity_{};

	// --- 通過済み ---
	bool passed_ = false;
	Vector3 escapeDirection_{}; // 離脱する向き(単位ベクトル)。通過した瞬間に決めて以後変えない
	float escapeTime_ = 0.0f;   // 通過済みになってからの経過時間[秒]

	// --- 全滅で落ちている最中(クリア演出) ---
	bool dying_ = false;
	float dyingTime_ = 0.0f;     // 落ち始めてからの経過時間[秒]
	float dyingDuration_ = 0.0f; // 消えるまでの長さ[秒]。個体ごとに散らして、一斉に消えず順に消えていくようにする
	float dyingSpin_ = 0.0f;     // 落ちながら転がる速さ[rad/s]
};
