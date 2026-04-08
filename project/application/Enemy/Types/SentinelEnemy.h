#pragma once
#include "../Core/BaseEnemy.h"

class SentinelEnemy : public BaseEnemy {
public:
	enum class State { Idle, Fleeing, Returning, Panicking };

	SentinelEnemy();
	~SentinelEnemy() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

	// --- 【重要】スリングショット移動のために必要なオーバーライド ---

	// 舌が刺さるように true を返す
	bool IsGrappable() const override { return true; }

	// 舌が当たった瞬間に呼ばれる（飛んできた方向を受け取る）
	void OnTongueHit(const Vector3& direction) override;

private:
	State state_ = State::Idle;
	Vector3 homePosition_;
	Vector3 panicDir_; // 逃げる方向（舌が飛んできた方向の延長線）
	float floatTimer_ = 0.0f;
	float panicTimer_ = 0.0f;

	float normalSpeed_ = 3.0f;
	float fleeSpeed_ = 12.0f;
	float panicSpeed_ = 45.0f; // パニック時の猛烈な加速（プレイヤーのブースターになる）
	float detectRange_ = 20.0f;
	float loseRange_ = 40.0f;
};