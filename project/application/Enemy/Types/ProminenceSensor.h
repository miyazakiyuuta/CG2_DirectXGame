#pragma once
#include "../../../engine/math/Vector3.h"
#include "../../../engine/math/Vector4.h"
#include "../Core/BaseEnemy.h"
#include <memory>

class Object3dCommon;
class Camera;

/// <summary>
/// プロミネンス・センサー：ガーディアン風の視覚システムを持つ固定砲台
/// </summary>
class ProminenceSensor : public BaseEnemy {
public:
	enum class SensorState {
		Idle,     // 索敵（青ライン・首振り）
		LockOn,   // ロックオン（赤ライン・追従）
		Charging, // 発射準備（狙い固定・赤点滅）
		Firing,   // レーザー発射
		Cooldown  // 再装填
	};

	ProminenceSensor();
	~ProminenceSensor() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void SetRotation(const Vector3& rot) override; // 【追加】エディタからの回転を反映
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

	bool IsFiring() const { return state_ == SensorState::Firing; }
	Vector3 GetBeamOrigin() const;
	Vector3 GetBeamDirection() const { return lockOnDir_; }
	float GetBeamRange() const { return kDetectRange; }
	float GetBeamRadius() const { return kBeamRadius; }

private:
	// 視野角（FOV）と距離、擬態を考慮した判定
	bool CanSeePlayer(const Vector3& playerPos);

	// ビームObject3dの座標・回転・スケールを計算して適用する
	void UpdateBeamTransform(const Vector3& origin, const Vector3& direction, float length, float thickness);

private:
	SensorState state_;
	float stateTimer_;

	// 各種設定定数 (constexprを使うことでMSVCのエラーを回避)
	static constexpr float kDetectRange = 45.0f;
	static constexpr float kLockOnTime = 1.5f;
	static constexpr float kChargeTime = 0.8f;
	static constexpr float kFireDuration = 0.15f;
	static constexpr float kCooldownTime = 2.5f;

	// 視野の設定（約60度）
	static constexpr float kViewHalfAngle = 0.523f;

	Vector3 lockOnDir_;
	Vector3 forwardDir_;     // 現在の視線方向
	Vector3 baseForwardDir_; // 【追加】索敵の基準となる正面方向
	float searchTimer_;      // 【追加】首振り計算用のタイマー
	static constexpr float kBeamRadius = 0.8f; //ビームの当たり判定用半径

	// --- ビームOBJモデル描画用 ---
	// ビーム形状のOBJモデルを描画するObject3d
	std::unique_ptr<Object3d> beamObject_;
	// ビームの可視状態（trueのとき描画する）
	bool beamVisible_ = false;
	// ビームの現在の色（ステートに応じて変化する）
	Vector4 beamColor_ = {0.2f, 0.6f, 1.0f, 0.4f};
};