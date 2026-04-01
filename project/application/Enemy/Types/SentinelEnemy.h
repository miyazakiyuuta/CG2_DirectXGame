#pragma once
#include "../Core/BaseEnemy.h"

/// <summary>
/// センチネル・エネミー：プレイヤーが近づくと素早く逃げ、見失うと元の場所に戻る
/// </summary>
class SentinelEnemy : public BaseEnemy {
public:
	SentinelEnemy();
	~SentinelEnemy() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

private:
	enum class State {
		Idle,     // 待機中
		Fleeing,  // 逃走中
		Returning // 帰還中
	};

	State state_ = State::Idle;
	Vector3 homePosition_; // 初期位置

	// 調整パラメータ
	float normalSpeed_ = 3.0f;  // 帰還時の速度
	float fleeSpeed_ = 12.0f;   // 逃走時の速度（速めにする）
	float detectRange_ = 20.0f; // 反応する距離（広めにする）
	float loseRange_ = 35.0f;   // 諦めて戻り始める距離
	float floatTimer_ = 0.0f;   // ふわふわ用
};