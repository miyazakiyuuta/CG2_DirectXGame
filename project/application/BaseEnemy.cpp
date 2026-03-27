#include "BaseEnemy.h"
#include "3d/Object3d.h"

// コンストラクタとデストラクタをここで定義することで unique_ptr<Object3d> を安全に扱う
BaseEnemy::BaseEnemy() = default;
BaseEnemy::~BaseEnemy() = default;

void BaseEnemy::ApplyGravity(float deltaTime) {
	// 秒間の重力をフレーム単位に変換して加算
	velocity_.y += gravity_ * deltaTime;
	// 速度を座標に加算
	position_.y += velocity_.y;

	// 【埋まり防止】設定された接地高さ(groundY_)で強制ストップ
	if (position_.y < groundY_) {
		position_.y = groundY_;
		velocity_.y = 0.0f;
	}
}