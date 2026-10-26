#pragma once
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Collision.h"

#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
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

	// 自機モデルのパスと表示スケール。タイトル画面の演出も同じ見た目で出すため公開する
	// (モデルを差し替えるときはPlayer.cppの1箇所を直せば、ゲームとタイトルの両方に反映される)
	static const std::string& ModelPath();
	static float ModelScale();

	void SetCamera(Camera* camera);
	void SetRail(const CatmullRomSpline* rail) { rail_ = rail; }
	// レール上の距離[m]。シーンが毎フレーム供給する(カメラ位置+前方オフセット)
	void SetRailDistance(float distance) { railDistance_ = distance; }

	// 被弾・射撃・移動を受け付けるか。シーンが毎フレーム供給する(エディタのEditモード中・ゲーム開始のイントロ中はfalse)。
	// Playerはシーンのモードを知らず、「被弾してよいか/撃ってよいか/動いてよいか」という自分の言葉で受け取る
	void SetDamageEnabled(bool enabled) { damageEnabled_ = enabled; }
	void SetShootEnabled(bool enabled) { shootEnabled_ = enabled; }
	void SetMoveEnabled(bool enabled) { moveEnabled_ = enabled; }

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
	/// チャージ弾の発射先。通常弾とは別インスタンスで、シーンが所有する(所有しない)。
	/// nullptrならチャージは溜まらない(長押ししても押した瞬間の通常弾1発だけになる)
	/// </summary>
	void SetChargeBulletManager(BulletManager* bulletManager) { chargeBulletManager_ = bulletManager; }

	/// <summary>
	/// 敵弾の一覧の取得元。シーンが所有する実体を指すだけ(所有しない)。
	/// nullptrなら敵弾では被弾しない。
	/// 判定をここ(撃たれる側)に置く理由: 無敵時間の管理がTakeDamageの1箇所に閉じているため、
	/// 当たった瞬間にそのまま適用できる(自弾→敵の判定をEnemySpawnerが持つのと同じ向き)
	/// </summary>
	void SetEnemyBulletManager(BulletManager* bulletManager) { enemyBulletManager_ = bulletManager; }

	/// <summary>
	/// ダメージを受ける。無敵時間中・死亡後・外から被弾を無効にされている間は何も起きない。
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
	/// <summary>
	/// 自機の進行方向(今いる地点のレールの接線、単位ベクトル)。
	/// 「自機→狙点」の向きは照準を動かすと横へ傾くので、進行方向が欲しいときはこちらを使う
	/// </summary>
	const Vector3& GetForward() const { return forward_; }

	// チャージが満タンか(射撃ボタンを押し続けた時間がチャージ時間に達したか)
	bool IsCharged() const { return chargeTimer_ >= chargeDuration_; }
	// チャージの溜まり具合(0=空 〜 1=満タン)
	float GetChargeRatio() const;

	/// <summary>
	/// 新しくロックできる状態か(チャージが満タンで、まだ誰もロックしていない)。
	/// 敵を探すのは敵の位置を知っているEnemySpawnerで、シーンが結果をSetLockTargetで渡す
	/// </summary>
	bool CanLockOn() const { return IsCharged() && lockTargetId_ == 0; }
	/// <summary>
	/// ロックする敵の番号を受け取る(0でロック解除)。Playerは敵そのものを知らず、番号だけを持つ。
	/// 離した瞬間にこの番号を付けてチャージ弾を撃ち、弾がその敵を追尾する。
	/// ロックしていない状態から新しくロックした瞬間にロック音を鳴らす(被弾音をTakeDamageで鳴らすのと同じく、
	/// 状態を持つPlayerが「変わった瞬間」の音も受け持つ)
	/// </summary>
	void SetLockTarget(uint32_t enemyId);
	// ロック中の敵の番号(0=ロックなし)
	uint32_t GetLockTargetId() const { return lockTargetId_; }
	// ロックの吸い付く範囲(照準からの角度[rad])と最大距離[m]。判定はEnemySpawnerが行い、ここは設定値を持つだけ
	float GetLockAssistAngle() const { return lockAssistAngle_; }
	float GetLockRange() const { return lockRange_; }
	// 今フレームでステージと接触しているか
	bool IsHit() const { return isHit_; }
	float GetCollisionRadius() const { return collisionRadius_; }

	/// <summary>
	/// スタート時点の状態に初期化
	/// 設定値は戻さない
	/// Initializeの後に呼ぶこと。シーンのResetボタンからも呼ぶ
	/// </summary>
	void Reset();

	Player();
	~Player();

private:
	/// <summary>
	/// レール上のdistance地点の座標系(前/右/上)を作る。
	/// 自機の位置合成と狙点の算出で同じ計算をするため関数に切り出している
	/// (狙点はカーブでもレールに沿うよう、自機地点ではなく狙点地点の軸で組む)
	/// </summary>
	void ComputeRailFrame(float distance, Vector3& forward, Vector3& right, Vector3& up) const;

	// 射撃入力を読み、押した瞬間に通常弾を1発撃ち、押し続けている間はチャージを溜め、
	// 満タンで離すとチャージ弾を撃つ。worldPosition_と狙点が確定した後に呼ぶこと
	void UpdateShooting(float deltaTime);

	// 弾の発射方向(銃口→狙点)。通常弾とチャージ弾で同じ向きに撃つため1箇所にまとめる
	Vector3 ComputeShotDirection() const;

	// チャージを溜められるか。発射先が無い間と、チャージ弾が飛んでいる
	// (容量1のプールが埋まっている)間は溜めない=同時に飛ぶのは1発まで
	bool CanCharge() const;

	// 敵弾との当たり判定。当たった弾を消してTakeDamageを呼ぶ。
	// ワールド位置が確定した後、無敵時間を消化した後に呼ぶこと
	void UpdateEnemyBulletCollision();

	// 今、被弾を受け付けるか(死亡後・無敵中・外から無効にされている間はfalse)。
	// 被弾を受け付けない理由を1箇所に集約し、TakeDamageと被弾回数の数え方がずれないようにする
	bool CanTakeDamage() const { return !IsDead() && !IsInvincible() && damageEnabled_; }

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
	// 移動の許可(SetMoveEnabledで外から渡される)。
	// 状態でも設定でもない「外からの許可」なので、Reset()では戻さない
	bool moveEnabled_ = true;

	// --- 当たり判定(ステージコライダーとの接触) ---
	// 判定形状は球。見た目のscale(0.5)と合わせる
	float collisionRadius_ = 0.5f;
	// Stageが所有するワールドAABB一覧への参照(所有しない)
	const std::vector<AABB>* stageColliders_ = nullptr;
	// 今フレームの接触状態と接触数(ImGui表示用)
	bool isHit_ = false;
	int hitCount_ = 0;

	// --- HPと無敵時間 ---
	// 最大HP（上限）で、発生時のHPも兼ねる
	// Resetでhp_に複製するので、ImGuiで変えた値が次のプレイに効く
	int maxHp_ = 3;
	int hp_ = 0;
	// 被弾後に無敵になる時間[秒]と残り時間。
	// なぜ要るか: 地形は接触している間ずっと当たり続けるため、
	// 無いと1回の接触で毎フレームHPが減り、一瞬で溶ける
	float invincibleDuration_ = 1.5f;
	float invincibleTimer_ = 0.0f;
	// 無敵中の点滅周期[秒]。この半分の間だけ描画する
	float blinkInterval_ = 0.1f;
	// 被弾の許可(SetDamageEnabledで外から渡される)。
	// 状態でも設定でもない「外からの許可」なので、Reset()では戻さない
	bool damageEnabled_ = true;

	// --- 射撃 ---
	// 弾の発射先(シーンが所有。所有しない)
	BulletManager* bulletManager_ = nullptr;
	// チャージ弾の発射先(シーンが所有。所有しない)
	BulletManager* chargeBulletManager_ = nullptr;
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
	// 1.0=レール接線と平行に飛ぶ / 0.0=自機がどこにいてもレール中心線へ完全収束 / 1より大きい=外側へ広がる。
	// 狙点は自機より遠い(カメラから約42m、自機は約12m)ので、画面上でレティクルを自機並みに動かすには
	// 1より大きい値が要る。2.5で自機の動く範囲の約7割(3.5で完全に同じだが、レティクルが常に機体に重なって見えにくい)
	float aimConvergence_ = 2.5f;
	// 銃口を自機の何m前に置くか(自機モデルの中から弾が湧いて見えないようにする)
	float muzzleForwardOffset_ = 1.0f;
	// 連打の最短間隔[秒]と残りクールタイム。押した瞬間に1発撃つ方式なので普段の連打では効かないが、
	// RTを半押しで止めたときに押下判定がしきい値付近で揺れ、1回のつもりで何発も出るのを抑える
	float shootInterval_ = 0.08f;
	float shootCooldown_ = 0.0f;
	// 前フレームで射撃ボタンが押されていたか。押した瞬間・離した瞬間は今フレームとの比較で求める
	// (RTはアナログ値で瞬間判定が無いため、ActionInput::IsTriggerではなくIsPressの変化を自分で見る)
	bool wasShootPressed_ = false;
	// 射撃の許可(SetShootEnabledで外から渡される)。
	// 状態でも設定でもない「外からの許可」なので、Reset()では戻さない
	bool shootEnabled_ = true;

	// --- チャージ(射撃ボタンの長押し) ---
	// 押し続けている時間[秒]。満タン(chargeDuration_)で頭打ちにし、離すと0に戻る
	float chargeTimer_ = 0.0f;
	// 満タンになるまでの時間[秒]。短すぎると連打の合間に溜まってしまう
	float chargeDuration_ = 0.7f;

	// --- ロックオン(チャージ満タン中に照準を敵へ重ねる) ---
	// ロック中の敵の番号(0=ロックなし)。押している間は照準を外しても保ち、
	// 離す・撃てなくなる・Resetで外れる(敵が居なくなったときはシーンが外す)
	uint32_t lockTargetId_ = 0;
	// 吸い付く範囲。照準から敵の縁までがこの角度[rad]以内なら、チャージ中に照準がその敵へ引き寄せられ、
	// 満タンでロックする。0なら照準を敵に重ねたときだけ、約30度(画面の端まで)にすると完全な自動照準になる。
	// 既定は照準の近くだけに吸い付く半自動(連打で狙う通常弾との差を残しつつ、狙いの甘さを補う)
	float lockAssistAngle_ = 8.0f * std::numbers::pi_v<float> / 180.0f;
	// ロックできる最大距離[m](カメラから)。チャージ弾の射程(125m)より短くし、届かない敵をロックしない
	float lockRange_ = 100.0f;

	// 被弾音。SoundManagerのキャッシュと共有し、再生中のPCMを有効に保つ
	std::shared_ptr<const SoundData> damageSound_;
	// チャージが満タンになった瞬間の音と、ロックした瞬間の音(持ち方は被弾音と同じ)
	std::shared_ptr<const SoundData> chargeReadySound_;
	std::shared_ptr<const SoundData> lockOnSound_;

	// 仮モデル(見た目は後で差し替える)
	std::unique_ptr<Object3d> object3d_;
};
