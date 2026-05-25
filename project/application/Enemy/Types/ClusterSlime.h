#pragma once
#include "../Core/BaseEnemy.h"
#include <memory>
#include <vector>

/// <summary>
/// クラスタースライム：地面を独立して這い回り、プレイヤーを包囲する群れエネミー
/// </summary>
class ClusterSlime : public BaseEnemy {
public:
	ClusterSlime();
	~ClusterSlime() override;

	void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) override;
	void Update(float deltaTime, const Vector3& playerPos) override;
	void Draw() override;

	std::vector<TargetPart> GetTargetParts(float radius) const override;
	void KillPart(int partId) override;

private:
	struct Member {
		std::unique_ptr<Object3d> object;
		Vector3 position;
		Vector3 velocity;
		float speed;
		float timer;
		bool onGround; // 接地フラグ
		bool isDead = false; // 個別死亡フラグ

		// メンバーごとの着地ブロック XZ 範囲と上面 Y を記録する
		// BaseEnemy の hasLandingBlock_ は共有されるため、メンバー切り替え時に退避・復元する
		bool hasLandingBlock = false;
		float landBlockMinX = 0.0f;
		float landBlockMaxX = 0.0f;
		float landBlockMinZ = 0.0f;
		float landBlockMaxZ = 0.0f;
		float landBlockTopY = 0.0f;  // ブロック上面 Y（プレイヤーが実際に乗っているか判定に使用）
	};

	std::vector<Member> members_;

	// 群れの制御パラメータ
	float detectRadius_ = 30.0f; // プレイヤーに気づく距離
	float personalSpace_ = 1.8f; // 個体同士が離れようとする距離
	float groundY_ = 0.4f;       // 地面の高さ
};