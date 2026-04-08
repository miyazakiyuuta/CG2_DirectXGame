#pragma once
#include "../Core/BaseEnemy.h"
#include <memory>

/// <summary>
/// フェイズ・ゴースト：通常は透明・不可視、ソナーでのみ実体化する
/// </summary>
class PhaseGhost : public BaseEnemy {
public:
	PhaseGhost();
	~PhaseGhost() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

	// ソナー通知を受け取った際の処理
	void OnSonarHit() override;

	// 実体化している間だけフック可能にする
	bool IsGrappable() const override { return isMaterialized_; }

private:
	bool isMaterialized_ = false;            // 現在実体化しているか
	float materializeTimer_ = 0.0f;          // 実体化持続タイマー
	const float kMaterializeDuration = 3.0f; // 実体化する秒数

	Vector3 homePos_;         // 出現位置
	float floatTimer_ = 0.0f; // ふわふわ移動用
};