#include "enemy/Enemy.h"

#include "3d/Object3d.h"
#include "math/Matrix4x4.h"
#include "math/Vector4.h"

#include <algorithm>
#include <cmath>

Enemy::~Enemy() = default;

void Enemy::Initialize(uint32_t id, size_t spawnIndex, size_t typeIndex, std::unique_ptr<Object3d> object,
	const EnemyTypeParams& params, const Context& context) {
	id_ = id;
	spawnIndex_ = spawnIndex;
	typeIndex_ = typeIndex;
	object_ = std::move(object);
	position_ = object_->GetTranslate();
	hp_ = params.maxHp;
	ApplyHpColor(params.maxHp);

	// 種別ごとの準備は最後に呼ぶ(共通の状態がそろった後なら、派生側は位置やHPを前提にできる)
	OnSpawn(params, context);
}

void Enemy::OnSpawn(const EnemyTypeParams&, const Context&) {
	// 既定では準備は要らない
}

void Enemy::Approach(const EnemyTypeParams& params, const Vector3& target, float deltaTime) {
	const Vector3 toTarget = target - position_;
	const float distance = std::sqrt(toTarget.LengthSquared());
	// 止まっている間の速度は0(通過済みになった瞬間に、離脱の初速として引き継ぐ)
	Hold();
	if (distance <= params.keepDistance || distance <= 1.0e-4f) {
		return;
	}
	const Vector3 direction = Vector3::Normalized(toTarget);
	// 行き過ぎて反転しないよう、残り距離を超えて進まないようにする
	const float step = (std::min)(params.moveSpeed * deltaTime, distance - params.keepDistance);
	position_ += direction * step;
	object_->SetTranslate(position_);
	velocity_ = direction * params.moveSpeed;
}

void Enemy::ApplyDamage(int damage, int maxHp) {
	// 0未満にはしない(HP0=撃破という意味を、大ダメージでも崩さない)
	hp_ = (std::max)(hp_ - damage, 0);
	if (hp_ > 0) {
		ApplyHpColor(maxHp); // 残りHPを色へ反映(撃破時は消えるので不要)
	}
}

void Enemy::ApplyHpColor(int maxHp) {
	// HP満タンで白、減るほど赤へ寄せる。残量が常に見えるのでタイマー付きの点滅が要らない
	// 最大HPは種別ごとに違うので、色の基準もその個体の種別から引く
	const float ratio = maxHp > 0
		? std::clamp(static_cast<float>(hp_) / static_cast<float>(maxHp), 0.0f, 1.0f)
		: 1.0f;
	const float rest = 0.2f + 0.8f * ratio; // HP0付近で0.2、満タンで1.0
	object_->SetColor({ 1.0f, rest, rest, 1.0f });
}

void Enemy::StartEscape(const Vector3& direction) {
	passed_ = true;
	escapeDirection_ = direction;
}

void Enemy::UpdateEscape(float deltaTime, float brake, float speed) {
	// 追っていた勢いはブレーキで落とし(0で止め、逆向きにはしない)、外側へ抜ける速さを足す。
	// 自機はレール上を前進し続けるので、勢いが落ちた敵は自然と後ろへ置いていかれる
	const float currentSpeed = velocity_.Length();
	const float brakedSpeed = (std::max)(currentSpeed - brake * deltaTime, 0.0f);
	velocity_ = currentSpeed > 1.0e-4f ? velocity_ * (brakedSpeed / currentSpeed) : Vector3{ 0.0f, 0.0f, 0.0f };
	position_ += (velocity_ + escapeDirection_ * speed) * deltaTime;
	object_->SetTranslate(position_);
	escapeTime_ += deltaTime;
}

void Enemy::StartDying(float duration, const Vector3& velocity, float spin, const Vector4& color) {
	dying_ = true;
	dyingTime_ = 0.0f;
	dyingDuration_ = duration;
	velocity_ = velocity;
	dyingSpin_ = spin;
	object_->SetColor(color);
}

void Enemy::UpdateDying(float deltaTime, float gravity) {
	// 落ちる。重力で速度を積み、転がりながら下へ
	dyingTime_ += deltaTime;
	velocity_.y -= gravity * deltaTime;
	position_ += velocity_ * deltaTime;
	object_->SetTranslate(position_);
	Vector3 rotate = object_->GetRotate();
	rotate.x += dyingSpin_ * deltaTime;
	object_->SetRotate(rotate);
}

void Enemy::UpdateObject(float deltaTime) {
	object_->Update(deltaTime);
}

void Enemy::Draw() {
	object_->Draw();
}

void Enemy::SetCamera(Camera* camera) {
	object_->SetCamera(camera);
}

std::unique_ptr<Object3d> Enemy::ReleaseObject() {
	return std::move(object_);
}

Vector3 Enemy::HitCenter(const EnemyTypeParams& params) const {
	return ForwardPoint(position_, object_->GetRotate(), params.hitOffset);
}

Vector3 Enemy::ForwardPoint(const Vector3& position, const Vector3& rotate, float offset) {
	if (offset == 0.0f) {
		return position; // 回転の計算を省く(箱の敵はすべてここを通る)
	}
	// モデルの規格は頭が-Z。ローカルの-Zに回転を掛けたものが、ワールドでの頭の向き
	// (行ベクトル規約では、ローカルの軸に回転行列を掛けるとワールドでの軸になる)
	const Vector3 head = Matrix4x4::Rotate(rotate).Transform({ 0.0f, 0.0f, -1.0f });
	return position + head * offset;
}

Transform Enemy::GetPose() const {
	Transform pose = object_->GetTransform();
	pose.translate = position_;
	return pose;
}
