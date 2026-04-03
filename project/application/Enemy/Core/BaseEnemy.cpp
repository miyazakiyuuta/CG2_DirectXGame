#include "BaseEnemy.h"
#include "3d/Object3d.h"

BaseEnemy::BaseEnemy() = default;
BaseEnemy::~BaseEnemy() = default;

void BaseEnemy::ApplyGravity(float deltaTime) {
	// 速度に重力を加算 (秒間加速度をフレーム単位へ)
	velocity_.y += gravity_ * deltaTime;
	// 位置に速度を加算
	position_.y += velocity_.y;

	// 【接地判定】設定された groundY_ より下に行かないようにする
	if (position_.y < groundY_) {
		position_.y = groundY_;
		velocity_.y = 0.0f;
	}

}

void BaseEnemy::SetColor(const Vector4& color){
	if(object_) object_->SetColor(color);
	originalColor_ = color;
}

void BaseEnemy::SetAlpha(float a){
	if(object_){
		Vector4 c = originalColor_;
		c.w = a;
		object_->SetColor(c);
	}
}

float BaseEnemy::GetOriginalAlpha() const {
	return originalColor_.w;
}
