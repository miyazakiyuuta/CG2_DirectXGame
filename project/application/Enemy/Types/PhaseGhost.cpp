#include "PhaseGhost.h"
#include "../../../engine/3d/Object3d.h"
#include <algorithm>
#include <cmath>

PhaseGhost::PhaseGhost() = default;
PhaseGhost::~PhaseGhost() = default;

void PhaseGhost::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("GhostFace.obj");
	object_->SetCamera(camera);

	position_ = pos;
	homePos_ = pos;
	object_->SetScale({1.2f, 1.2f, 1.2f});

	// 初期状態は極めて透明
	SetColor({0.6f, 0.8f, 1.0f, 0.05f});

	// 浮遊するので重力なし
	gravity_ = 0.0f;
	groundY_ = -100.0f;
}

void PhaseGhost::OnSonarHit() {
	// ソナーが当たったら実体化フラグを立て、タイマーをリセット
	isMaterialized_ = true;
	materializeTimer_ = kMaterializeDuration;
}

void PhaseGhost::Update(float deltaTime, const Vector3& playerPos) {
	(void)playerPos;

	// 1. 実体化タイマーの更新
	if (isMaterialized_) {
		materializeTimer_ -= deltaTime;
		if (materializeTimer_ <= 0.0f) {
			isMaterialized_ = false;
		}
	}

	// 2. 状態による見た目の変化
	if (isMaterialized_) {
		// 実体化中：青白く発光させ、不透明にする
		SetAlpha(1.0f);
		object_->SetColor({1.2f, 1.5f, 2.0f, 1.0f});
	} else {
		// 非実体化：ほぼ透明に戻る
		SetAlpha(0.05f);
		object_->SetColor({0.6f, 0.8f, 1.0f, 0.05f});
	}

	// 3. ふわふわとした浮遊AI
	floatTimer_ += deltaTime;
	// サイン波を用いて三次元的にゆらゆら動かす
	position_.x = homePos_.x + std::sin(floatTimer_ * 1.0f) * 2.0f;
	position_.y = homePos_.y + std::cos(floatTimer_ * 1.5f) * 1.5f;
	position_.z = homePos_.z + std::sin(floatTimer_ * 0.5f) * 2.0f;

	if (object_) {
		object_->SetTranslate(position_);
		object_->SetRotate({0, floatTimer_ * 0.3f, 0});
	}
}

void PhaseGhost::Draw() {
	object_->Update();
	if (object_)
		object_->Draw();
}