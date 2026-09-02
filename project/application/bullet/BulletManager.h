#pragma once
#include "bullet/Bullet.h"
#include "math/Collision.h"

#include <cstddef>
#include <memory>
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
	/// プールを確保する。ここでcapacity個のObject3dをまとめて生成し、以降は生成しない
	/// </summary>
	/// <param name="capacity">同時に存在できる弾数。超過分の発射は無視される</param>
	void Initialize(size_t capacity = 64);

	/// <summary>
	/// 空きスロットを1つ使って発射する。空きが無ければ何もしない
	/// (弾切れで撃てないのは仕様。無音で消えるより上限に張り付く方が調整しやすい)
	/// </summary>
	/// <param name="position">発射位置(銃口のワールド座標)</param>
	/// <param name="direction">進行方向。正規化されていなくてよい(内部で正規化する)</param>
	void Fire(const Vector3& position, const Vector3& direction);

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

	// 生存中の弾を全て消す(レール一周・Resetボタン用)。実体は消さないのでいつ呼んでも安全
	void Clear();

	// "Bullet"ウィンドウに生存数と速度・寿命・半径を表示する(Debug構成のみ)
	void DrawImGui();

	/// <summary>
	/// 弾の一覧。A-4で敵側が被弾判定に使う(activeなものだけを見ること)
	/// </summary>
	const std::vector<Bullet>& GetBullets() const { return bullets_; }

	/// <summary>
	/// index番の弾を消す。A-4で敵に当たった弾を消すための窓口
	/// </summary>
	void Kill(size_t index);

	// 判定半径(自弾→敵の判定でも同じ値を使う)
	float GetRadius() const { return radius_; }

	BulletManager();
	~BulletManager();

private:
	// 弾の状態。要素数はInitialize後に変わらない(indexがスロット番号そのもの)
	std::vector<Bullet> bullets_;
	// bullets_と同じ並びの見た目(indexで対応)。activeでない弾の分もここに残り続ける
	std::vector<std::unique_ptr<Object3d>> objects_;

	// Stageが所有するワールドAABB一覧への参照(所有しない)
	const std::vector<AABB>* stageColliders_ = nullptr;
	Camera* camera_ = nullptr;

	// 発射音。SoundManagerのキャッシュと共有し、発射のたびにPCMをコピーしない。
	// ヘッダにxaudio2.hを引き込まないようSoundDataは前方宣言のまま持つ
	std::shared_ptr<const SoundData> shotSound_;

	// --- 全弾共通のパラメータ(ImGuiで調整する) ---
	float speed_ = 80.0f;    // 初速[m/s]
	float lifeTime_ = 2.0f;  // 寿命[秒](= 射程160m)
	float radius_ = 0.3f;    // 判定半径[m]。見た目のscaleと合わせる

	// ImGui表示用の統計(今フレームの生存数と、通算の発射数・地形ヒット数)
	size_t aliveCount_ = 0;
	int firedCount_ = 0;
	int terrainHitCount_ = 0;
};
