#pragma once
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Collision.h"

#include <memory>
#include <vector>

class ActionInput;
class BulletManager;
class Camera;
class CatmullRomSpline;
class Object3d;
struct SoundData;

// 自機。ワールド座標を直接持たず「レール上の距離 + レール断面内のローカルオフセット(x,y)」で
// 位置を表し、毎フレーム ワールド位置 = レール基準点 + 右×x + 上×y を合成する。
// なぜ: カメラ・照準・衝突がすべてこの分解を前提にするため
// (ワールド直持ちで作ると後で作り直しになる)。
// レール進行度の所有はシーン側(進行度=ステージの時間軸で敵スポーン等も使う)。
// SetRailDistance で毎フレーム供給を受ける。
class Player {
public:
	void Initialize(ActionInput* actionInput);
	void Update(float deltaTime);
	void Draw();

	// "Player"ウィンドウにオフセット・速度・アクション押下状態を表示
	void DrawImGui();

	void SetCamera(Camera* camera);
	void SetRail(const CatmullRomSpline* rail) { rail_ = rail; }
	// レール上の距離[m]。シーンが毎フレーム供給する(カメラ位置+前方オフセット)
	void SetRailDistance(float distance) { railDistance_ = distance; }

	/// <summary>
	/// 当たり判定の相手(ステージのワールドAABB一覧)を渡す。
	/// Stageが所有する実体を指すだけなのでコピーしない(毎フレーム作り直されるため)。
	/// nullptrなら判定を行わない
	/// </summary>
	void SetStageColliders(const std::vector<AABB>* colliders) { stageColliders_ = colliders; }

	/// <summary>
	/// 弾の発射先。シーンが所有する実体を指すだけ(所有しない)。
	/// nullptrなら射撃入力を読んでも何も撃たない
	/// </summary>
	void SetBulletManager(BulletManager* bulletManager) { bulletManager_ = bulletManager; }

	/// <summary>
	/// 敵弾の一覧の取得元。シーンが所有する実体を指すだけ(所有しない)。
	/// nullptrなら敵弾では被弾しない。
	/// 判定をここ(撃たれる側)に置く理由: 無敵時間の管理がTakeDamageの1箇所に閉じているため、
	/// 当たった瞬間にそのまま適用できる(自弾→敵の判定をEnemySpawnerが持つのと同じ向き)
	/// </summary>
	void SetEnemyBulletManager(BulletManager* bulletManager) { enemyBulletManager_ = bulletManager; }

	/// <summary>
	/// ダメージを受ける。無敵時間中と死亡後は何も起きない。
	/// 地形との接触はUpdate内で自分に適用し、敵との接触はシーンがここを呼ぶ
	/// </summary>
	/// <param name="amount">減らすHP量</param>
	void TakeDamage(int amount = 1);

	int GetHp() const { return hp_; }
	int GetMaxHp() const { return maxHp_; }
	// HPが尽きたか
	bool IsDead() const { return hp_ <= 0; }
	// 無敵時間中か
	bool IsInvincible() const { return invincibleTimer_ > 0.0f; }

	float GetRailDistance() const { return railDistance_; }
	const Vector2& GetOffset() const { return offset_; }
	// 合成済みのワールド位置(照準・衝突の起点になる)
	const Vector3& GetWorldPosition() const { return worldPosition_; }

	/// <summary>
	/// 弾が向かう狙点(ワールド座標)。レティクルはこの点をスクリーン投影して描く。
	/// 同じ点を弾とレティクルで共有することで、両者が必ず一致する
	/// </summary>
	const Vector3& GetAimPoint() const { return aimPoint_; }
	// 銃口(弾の発射位置)。撃った瞬間のマズルフラッシュ等もここを使う
	const Vector3& GetMuzzlePosition() const { return muzzlePosition_; }
	// 今フレームでステージと接触しているか
	bool IsHit() const { return isHit_; }
	float GetCollisionRadius() const { return collisionRadius_; }

	Player();
	~Player();

private:
	/// <summary>
	/// レール上のdistance地点の座標系(前/右/上)を作る。
	/// 自機の位置合成と狙点の算出で同じ計算をするため関数に切り出している
	/// (狙点はカーブでもレールに沿うよう、自機地点ではなく狙点地点の軸で組む)
	/// </summary>
	void ComputeRailFrame(float distance, Vector3& forward, Vector3& right, Vector3& up) const;

	// 射撃入力を読み、クールタイムを消化していれば1発撃つ。
	// worldPosition_と狙点が確定した後に呼ぶこと
	void UpdateShooting(float deltaTime);

	// 敵弾との当たり判定。当たった弾を消してTakeDamageを呼ぶ。
	// ワールド位置が確定した後、無敵時間を消化した後に呼ぶこと
	void UpdateEnemyBulletCollision();

	ActionInput* actionInput_ = nullptr;
	const CatmullRomSpline* rail_ = nullptr;

	// レール上の距離[m](シーンから供給)とレール断面内のローカルオフセット(x=右+, y=上+)
	float railDistance_ = 0.0f;
	Vector2 offset_ = { 0.0f, 0.0f };
	// 今フレームの合成結果
	Vector3 worldPosition_ = { 0.0f, 0.0f, 0.0f };

	// 画面内の移動速度[m/s]とオフセットの可動範囲(±値でクランプ)
	float moveSpeed_ = 8.0f;
	Vector2 offsetLimit_ = { 4.0f, 2.5f };

	// --- 当たり判定(ステージコライダーとの接触) ---
	// 判定形状は球。見た目のscale(0.5)と合わせる
	float collisionRadius_ = 0.5f;
	// Stageが所有するワールドAABB一覧への参照(所有しない)
	const std::vector<AABB>* stageColliders_ = nullptr;
	// 今フレームの接触状態と接触数(ImGui表示用)
	bool isHit_ = false;
	int hitCount_ = 0;

	// --- HPと無敵時間 ---
	// 発生時のHP。Initializeでhp_へ複製する(ImGuiでMax HPを変えた値がリトライ時に効く)
	int maxHp_ = 3;
	//int maxHp_ = 30;
	int hp_ = 3;
	// 被弾後に無敵になる時間[秒]と残り時間。
	// なぜ要るか: 地形は接触している間ずっと当たり続けるため、
	// 無いと1回の接触で毎フレームHPが減り、一瞬で溶ける
	float invincibleDuration_ = 1.5f;
	float invincibleTimer_ = 0.0f;
	// 無敵中の点滅周期[秒]。この半分の間だけ描画する
	float blinkInterval_ = 0.1f;

	// --- 射撃(A-1) ---
	// 弾の発射先(シーンが所有。所有しない)
	BulletManager* bulletManager_ = nullptr;
	// 敵弾の一覧の取得元(シーンが所有。所有しない)
	BulletManager* enemyBulletManager_ = nullptr;
	// 敵弾で被弾した通算回数(ImGui表示用。無敵で弾かれた分は数えない)
	int enemyBulletHitCount_ = 0;
	// 今フレームの狙点と銃口(Updateで算出し、弾の方向とレティクルが共有する)
	Vector3 aimPoint_ = { 0.0f, 0.0f, 0.0f }; // 狙点(ワールド座標)
	Vector3 muzzlePosition_ = { 0.0f, 0.0f, 0.0f }; // 弾の出所
	// 今フレームの前方向(レール接線)。レール終端では狙点が手前にクランプされるため、
	// 発射方向が縮退・反転したときのフォールバックに使う
	Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
	// 狙点をレール上の何m先に置くか
	float aimDistance_ = 30.0f;
	// 狙点に自機のオフセットをどれだけ反映するか。
	// 1.0=レール接線と平行に飛ぶ / 0.0=自機がどこにいてもレール中心線へ完全収束
	float aimConvergence_ = 0.5f;
	// 銃口を自機の何m前に置くか(自機モデルの中から弾が湧いて見えないようにする)
	float muzzleForwardOffset_ = 1.0f;
	// 連射間隔[秒]と残りクールタイム。
	// IsTriggerではなくIsPress+クールタイムで撃つ(RTはアナログ値で瞬間判定が無いため)
	float shootInterval_ = 0.12f;
	float shootCooldown_ = 0.0f;

	// 被弾音。SoundManagerのキャッシュと共有し、再生中のPCMを有効に保つ
	std::shared_ptr<const SoundData> damageSound_;

	// 仮モデル(見た目は後で差し替える)
	std::unique_ptr<Object3d> object3d_;
};
