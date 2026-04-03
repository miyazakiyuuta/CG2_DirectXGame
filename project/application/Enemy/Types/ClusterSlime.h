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

private:
	struct Member {
		std::unique_ptr<Object3d> object;
		Vector3 position;
		Vector3 velocity;
		float speed;
		float timer;
		bool onGround; // 【追加】接地フラグ
	};

	std::vector<Member> members_;

	// 群れの制御パラメータ
	float detectRadius_ = 30.0f; // プレイヤーに気づく距離
	float personalSpace_ = 1.8f; // 個体同士が離れようとする距離
	float groundY_ = 0.4f;       // 地面の高さ
};