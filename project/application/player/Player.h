#pragma once
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Collision.h"

#include <memory>
#include <vector>

class ActionInput;
class Camera;
class CatmullRomSpline;
class Object3d;

// 自機。ワールド座標を直接持たず「レール上の距離 + レール断面内のローカルオフセット(x,y)」で
// 位置を表し、毎フレーム ワールド位置 = レール基準点 + 右×x + 上×y を合成する。
// なぜ: カメラ・照準(⑪)・衝突(⑩)がすべてこの分解を前提にするため
// (ワールド直持ちで作ると後で作り直しになる)。
// レール進行度の所有はシーン側(進行度=ステージの時間軸で敵スポーン等も使う)。
// SetRailDistance で毎フレーム供給を受ける。
class Player {
public:
	void Initialize(ActionInput* actionInput);
	void Update(float deltaTime);
	void Draw();

	// "Player"ウィンドウにオフセット・速度・アクション押下状態を表示する(Debug構成のみ)
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

	float GetRailDistance() const { return railDistance_; }
	const Vector2& GetOffset() const { return offset_; }
	// 合成済みのワールド位置(照準・衝突の起点になる)
	const Vector3& GetWorldPosition() const { return worldPosition_; }
	// 今フレームでステージと接触しているか(可視化の色分け等に使う)
	bool IsHit() const { return isHit_; }
	float GetCollisionRadius() const { return collisionRadius_; }

	Player();
	~Player();

private:
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

	// 仮モデル(見た目は後で差し替える)
	std::unique_ptr<Object3d> object3d_;
};
