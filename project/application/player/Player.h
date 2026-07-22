#pragma once
#include "math/Vector2.h"
#include "math/Vector3.h"

#include <memory>

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

	float GetRailDistance() const { return railDistance_; }
	const Vector2& GetOffset() const { return offset_; }
	// 合成済みのワールド位置(照準・衝突の起点になる)
	const Vector3& GetWorldPosition() const { return worldPosition_; }

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

	// 仮モデル(見た目は後で差し替える)
	std::unique_ptr<Object3d> object3d_;
};
