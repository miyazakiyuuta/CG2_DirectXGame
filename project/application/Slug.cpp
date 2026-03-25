#include "Slug.h"
#include "3d/Object3d.h"
#include "debug/DebugGrid.h"
#include <algorithm>
#include <cmath>

// コンストラクタ
Slug::Slug()
    : dxCommon_(nullptr)
	, object3dCommon_(nullptr)
	, camera_(nullptr)
	, position_(Vector3(0.0f, 0.0f, 0.0f))
	, yaw_(0.0f)
	, speed_(1.2f)
	, bodyColor_(Vector4(0.8f, 1.0f, 0.6f, 1.0f))
	,trailColor_(Vector4(0.0f, 1.0f, 1.0f, 1.0f)) // 初期の跡の色を鮮やかなネオンシアン(不透明)に
      ,
      timer_(0.0f), lastTrailPos_(Vector3(0.0f, -100.0f, 0.0f)), kTrailInterval(0.6f) // 跡を置く間隔
      ,
      kTrailFadeSpeed(0.25f) // 消える速さを少し調整
      ,
      kSnakeIntensity(0.015f) {}

Slug::~Slug() {}

void Slug::Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon, Camera* camera, const std::string& bodyModel) {
	dxCommon_ = dxCommon;
	object3dCommon_ = object3dCommon;
	camera_ = camera;

	// 本体
	body_ = std::make_unique<Object3d>();
	body_->Initialize(object3dCommon_);
	body_->SetModel(bodyModel);
	body_->SetCamera(camera_);
	body_->SetScale(Vector3(0.7f, 0.4f, 1.2f));

	float groundOffset = 0.4f;
	body_->SetTranslate(Vector3(position_.x, position_.y + groundOffset, position_.z));
	body_->Update();
}

void Slug::Update(float deltaTime) {
	if (!body_)
		return;

	// 1. 移動挙動
	timer_ += deltaTime;
	yaw_ += std::sin(timer_ * 0.8f) * kSnakeIntensity;

	Vector3 velocity = {std::sin(yaw_) * speed_ * deltaTime, 0.0f, std::cos(yaw_) * speed_ * deltaTime};

	position_.x += velocity.x;
	position_.z += velocity.z;

	float groundOffset = 0.4f;
	body_->SetTranslate(Vector3(position_.x, position_.y + groundOffset, position_.z));
	body_->SetRotate(Vector3(0.0f, yaw_, 0.0f));
	body_->SetColor(bodyColor_);
	body_->Update();

	// 2. 跡の生成
	float dx = position_.x - lastTrailPos_.x;
	float dz = position_.z - lastTrailPos_.z;
	if (dx * dx + dz * dz > kTrailInterval * kTrailInterval) {
		AddTrail();
		lastTrailPos_ = position_;
	}

	// 3. 跡のフェード更新
	for (auto it = trails_.begin(); it != trails_.end();) {
		it->lifespan -= kTrailFadeSpeed * deltaTime;

		if (it->lifespan <= 0.0f) {
			it = trails_.erase(it);
		} else {
			// 透明度(w)を lifespan で計算
			Vector4 currentTrailColor = trailColor_;
			currentTrailColor.w *= it->lifespan;
			it->grid->SetColor(currentTrailColor);

			++it;
		}
	}
}

void Slug::AddTrail() {
	TrailSegment segment;
	segment.grid = std::make_unique<DebugGrid>();

	// マス目を細かく(分割数4)して密集させることで目立たせる
	segment.grid->Initialize(dxCommon_, 0.4f, 4);
	segment.lifespan = 1.0f;

	segment.grid->SetPosition(Vector3(position_.x, position_.y + 0.05f, position_.z));
	segment.grid->SetColor(trailColor_);

	trails_.push_back(std::move(segment));
}

void Slug::Draw() {
	if (body_)
		body_->Draw();
}

void Slug::DrawTransparent(const Camera& camera) {
	for (auto& trail : trails_) {
		trail.grid->Draw(camera);
	}
}

void Slug::SetPosition(const Vector3& pos) {
	position_ = pos;
	if (body_) {
		body_->SetTranslate(Vector3(position_.x, position_.y + 0.4f, position_.z));
		body_->Update();
	}
}

void Slug::SetBodyColor(const Vector4& color) { bodyColor_ = color; }
void Slug::SetTrailColor(const Vector4& color) { trailColor_ = color; }