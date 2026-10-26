#pragma once
#include "bullet/Bullet.h"
#include "math/Collision.h"
#include "math/Vector4.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Camera;
class Object3d;
struct SoundData;

// 自弾のプール・更新・地形との衝突・描画をまとめて持つ。
// 生成/破棄をオブジェクトプールで回す(毎フレームnewしない)ことが要件。
// なぜプールか:
//  1. Object3dはGPU定数バッファを3本持つため、発射のたびに作ると生成コストとVRAM断片化が出る
//  2. 使い回す限りObject3dを一度もdeleteしないので、Stage/EnemySpawnerが必要としている
//     graveyard(遅延削除)が構造的に不要になる(描画コマンド記録済みの実体が消えない)
// 所有者はGamePlaySceneで、Playerには参照だけを渡す。
// なぜPlayerに持たせないか: 弾は撃った本人より長生きし、A-4(敵との判定)やA-5(敵弾)では
// プレイヤー以外からも一覧を読む必要があるため、シーンを唯一の所有者にしておく。
class BulletManager {
public:
	/// <summary>
	/// 弾の「性格」。自弾と敵弾で変わる分だけをまとめたもの。
	/// なぜ敵弾用のクラスを派生させないか: プール・地形判定・描画は自弾と完全に同一で、
	/// 違うのは見た目・音・速さというデータだけなので、型ではなくデータで分ける
	/// (パーティクルのParticleConfigと同じ「汎用システム+パラメータ駆動」の考え方)。
	/// 既定値は自弾の設定そのものなので、自弾側は何も渡さなくてよい
	/// </summary>
	struct Profile {
		// 仮モデル。半径0.5のsphereを前提に、判定半径からscaleを決める
		std::string model = "sphere.obj";
		// 弾の色(ライティングを切って自発光のように見せる)
		Vector4 color = { 0.4f, 1.0f, 1.0f, 1.0f };
		// 発射音。連射が速いのでカテゴリ音量とは別に個別で絞る
		// (SEカテゴリを下げると爆発音まで小さくなるため)
		std::string shotSound = "resources/sounds/shot.mp3";
		float shotVolume = 0.35f;
		float speed = 80.0f;   // 初速[m/s]
		float lifeTime = 2.0f; // 寿命[秒](速度×寿命=射程)
		float radius = 0.3f;   // 判定半径[m]。見た目のscaleと合わせる
		// 当たった相手に与えるダメージ(自弾→敵のHP、敵弾→自機のHP)
		int damage = 1;
		// 当たったときの爆発の半径[m]。0なら爆発せず、当たった相手だけにダメージを与える。
		// 巻き込み(半径内の敵へのダメージ)は敵の一覧を持つEnemySpawnerが行う
		float explosionRadius = 0.0f;
		// 追尾の強さ。1秒間に向きを変えられる最大の角度[度/秒]。
		// 追尾先の番号付きで撃たれた弾(Fireのtarget)にだけ効く
		float homingTurnRate = 0.0f;
		// --- 見た目と音の演出(空なら何もしない=自弾・敵弾は今までどおり) ---
		// パーティクルはParticleManagerのプリセット名で指定する。登録(RegisterEffect)は使う側(シーン)が
		// 弾を撃つより前に済ませておくこと(未登録の名前でEmitするとassertで止まる)
		// 飛んでいる間に出し続ける尾のプリセット名と、1秒あたりの粒数
		std::string trailEffect;
		float trailRate = 0.0f;
		// 弾が消えるとき(敵・地形に当たる、射程切れ)に出すプリセット名と粒数。
		// 敵に当たったときの「爆発の見た目」もここが担う(巻き込みのダメージはEnemySpawner)
		std::string impactEffect;
		int impactParticleCount = 0;
		// 弾が消えるときの音と音量
		std::string impactSound;
		float impactVolume = 1.0f;
		// ImGuiウィンドウ名。自弾と敵弾で必ず変えること:
		// 同名のBeginは1枚のウィンドウに合体し、同じラベルのスライダーがID衝突を起こす
		std::string debugName = "Bullet";
	};

	/// <summary>
	/// 番号から追尾先の位置を引く窓口。見つかればoutPositionに書いてtrue、居なければfalseを返す関数。
	/// 関数として受け取るのは、BulletManagerが敵の型(EnemySpawner)を知らずに追尾できるようにするため
	/// (敵を知らない作りだからこそ、自弾・敵弾・チャージ弾で同じクラスを使い回せている)
	/// </summary>
	using TargetResolver = std::function<bool(uint32_t targetId, Vector3& outPosition)>;

	/// <summary>
	/// プールを確保する。ここでcapacity個のObject3dをまとめて生成し、以降は生成しない
	/// </summary>
	/// <param name="capacity">同時に存在できる弾数。超過分の発射は無視される</param>
	/// <param name="profile">見た目・音・速さの設定。省略すると自弾の設定になる</param>
	void Initialize(size_t capacity = 64, const Profile& profile = {});

	/// <summary>
	/// 空きスロットを1つ使って発射する。空きが無ければ何もしない
	/// (弾切れで撃てないのは仕様。無音で消えるより上限に張り付く方が調整しやすい)
	/// </summary>
	/// <param name="position">発射位置(銃口のワールド座標)</param>
	/// <param name="direction">進行方向。正規化されていなくてよい(内部で正規化する)</param>
	/// <param name="targetId">追尾する相手の番号。0(省略時)なら追尾せずまっすぐ飛ぶ</param>
	void Fire(const Vector3& position, const Vector3& direction, uint32_t targetId = 0);

	void Update(float deltaTime);
	void Draw();

	// 描画に使うカメラを差し替える(プール内の全実体へ反映)
	void SetCamera(Camera* camera);

	/// <summary>
	/// 地形との判定相手(ステージのワールドAABB一覧)を渡す。
	/// Stageが所有する実体を指すだけなのでコピーしない(毎フレーム作り直されるため)。
	/// nullptrなら地形判定を行わず、弾は寿命で消えるだけになる
	/// </summary>
	void SetStageColliders(const std::vector<AABB>* colliders) { stageColliders_ = colliders; }

	// 追尾先の位置を引く窓口を渡す。未設定なら、番号付きで撃たれた弾もまっすぐ飛ぶ
	void SetTargetResolver(TargetResolver resolver) { targetResolver_ = std::move(resolver); }

	// 生存中の弾を全て消す(レール一周・Resetボタン用)。実体は消さないのでいつ呼んでも安全
	void Clear();

	// "Bullet"ウィンドウに生存数と速度・寿命・半径を表示する(Debug構成のみ)
	void DrawImGui();

	/// <summary>
	/// 弾の一覧。A-4で敵側が被弾判定に使う(activeなものだけを見ること)
	/// </summary>
	const std::vector<Bullet>& GetBullets() const { return bullets_; }

	/// <summary>
	/// index番の弾を消す。A-4で敵に当たった弾を消すための窓口。
	/// 地形・射程切れと同じく、Profileに消えるときの演出があれば出す
	/// </summary>
	void Kill(size_t index);

	// 判定半径(自弾→敵・敵弾→自機の判定でも同じ値を使う)
	float GetRadius() const { return profile_.radius; }
	// 1発あたりのダメージ
	int GetDamage() const { return profile_.damage; }
	// 爆発の半径[m](0なら爆発しない)
	float GetExplosionRadius() const { return profile_.explosionRadius; }

	/// <summary>
	/// 空きスロットがあるか(=今Fireすれば撃てるか)。
	/// チャージ弾は容量1で作るので、「飛んでいる間はチャージが溜まらない」の判定にそのまま使える
	/// </summary>
	bool HasFreeSlot() const;

	BulletManager();
	~BulletManager();

private:
	/// <summary>
	/// 弾が「消える」ときの共通処理。消えた位置にProfileの演出(パーティクル・音)を出す。
	/// 敵に当たる(Kill)・地形に当たる・射程切れの3つがここを通るので、どの消え方でも同じ見た目になる。
	/// Clear(レール一周・Reset)は演出を出さずに消すので、ここを通さない
	/// </summary>
	void Expire(size_t index);

	// 弾の状態。要素数はInitialize後に変わらない(indexがスロット番号そのもの)
	std::vector<Bullet> bullets_;
	// bullets_と同じ並びの見た目(indexで対応)。activeでない弾の分もここに残り続ける
	std::vector<std::unique_ptr<Object3d>> objects_;

	// Stageが所有するワールドAABB一覧への参照(所有しない)
	const std::vector<AABB>* stageColliders_ = nullptr;
	Camera* camera_ = nullptr;
	// 追尾先の位置を引く窓口(シーンが渡す)
	TargetResolver targetResolver_;

	// 発射音。SoundManagerのキャッシュと共有し、発射のたびにPCMをコピーしない。
	// ヘッダにxaudio2.hを引き込まないようSoundDataは前方宣言のまま持つ
	std::shared_ptr<const SoundData> shotSound_;
	// 弾が消えるときの音(Profileで指定が無ければnullptr=鳴らさない)
	std::shared_ptr<const SoundData> impactSound_;

	// 全弾共通のパラメータ(Initializeで受け取り、ImGuiで調整する)
	Profile profile_;

	// ImGui表示用の統計(今フレームの生存数と、通算の発射数・地形ヒット数)
	size_t aliveCount_ = 0;
	int firedCount_ = 0;
	int terrainHitCount_ = 0;
};
