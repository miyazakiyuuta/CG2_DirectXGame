#include "ProminenceSensor.h"
#include "../../../application/Player.h"
#include "../../../engine/3d/Object3d.h"
#include "../../../engine/debug/DebugRenderer.h"
#include <algorithm>
#include <cmath>

// --- コンストラクタ：変数の初期化 ---
ProminenceSensor::ProminenceSensor() : state_(SensorState::Idle), stateTimer_(0.0f), lockOnDir_({0, 0, 1}), forwardDir_({0, 0, 1}), baseForwardDir_({0, 0, 1}), searchTimer_(0.0f) {}

ProminenceSensor::~ProminenceSensor() = default;

// --- 初期化：見た目や物理挙動のセットアップ ---
void ProminenceSensor::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	// 3Dオブジェクトの基本設定
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	LoadModel("ProminenceSensor.obj");
	object_->SetCamera(camera);

	// 配置と大きさの設定
	position_ = pos;
	object_->SetTranslate(pos);
	object_->SetScale({1.2f, 1.2f, 1.2f});

	// 初期の向き（基準となる正面方向）を保存
	forwardDir_ = {0.0f, 0.0f, 1.0f};
	baseForwardDir_ = forwardDir_;

	// 重力・接地判定の初期化（BaseEnemyのメンバを使用）
	gravity_ = -0.02f;
	groundY_ = 0.0f;
}

// --- 更新処理：ステートマシンと索敵AI ---
void ProminenceSensor::Update(float deltaTime, const Vector3& playerPos) {
	if (isDead_) {
		UpdateDeathAnimation(deltaTime);
		return;
	}

	stateTimer_ += deltaTime; // 状態が変化してからの経過時間をカウント

	// 1. 物理計算（重力と当たり判定）
	// BaseEnemyで定義された共通の物理ロジックを使用
	Vector3 previousPosition = position_;
	velocity_.y += gravity_;
	position_.y += velocity_.y;

	// 地形との衝突解決
	ResolveHorizontalCollisions(previousPosition);
	ResolveVerticalCollisions();

	// レーザーの発射位置（「目」の高さ）を少し上に調整
	Vector3 eyePos = position_;
	eyePos.y += 0.6f;

	// 2. ステートマシン：現在の「状態」に応じて挙動を分岐させる
	switch (state_) {
	case SensorState::Idle:
		// 【索敵状態】サイン波を使って左右にゆっくり首を振る
		searchTimer_ += deltaTime;
		{
			// sin関数で角度を一定範囲（約45度）で揺らす
			float searchAngle = std::sin(searchTimer_ * 1.5f) * 0.8f;

			// 基準の向き(baseForwardDir)から回転後の向き(forwardDir)を計算
			forwardDir_.x = baseForwardDir_.x * std::cos(searchAngle) - baseForwardDir_.z * std::sin(searchAngle);
			forwardDir_.z = baseForwardDir_.x * std::sin(searchAngle) + baseForwardDir_.z * std::cos(searchAngle);
		}

		// 青い視線（デバッグライン）を表示して索敵中であることをアピール
		DebugRenderer::GetInstance()->AddLine(eyePos, eyePos + forwardDir_ * kDetectRange, {0.2f, 0.6f, 1.0f, 0.4f});

		// プレイヤーが視界内に入り、かつ擬態していなければ発見！
		if (CanSeePlayer(playerPos)) {
			state_ = SensorState::LockOn;
			stateTimer_ = 0;
			object_->SetColor({0.8f, 0.4f, 0.0f, 1.0f}); // 警戒色：オレンジ
		}
		break;

	case SensorState::LockOn:
		// 【追尾状態】首振りをやめて、赤いサイトでプレイヤーを執拗に追い続ける
		lockOnDir_ = Vector3::Normalized(playerPos - eyePos);
		DebugRenderer::GetInstance()->AddLine(eyePos, eyePos + lockOnDir_ * kDetectRange, {1.0f, 0.0f, 0.0f, 0.6f});

		{
			// ロック解除判定：一度見つかったら「隠れる（擬態）」か「遠くへ逃げる」まで離さない
			bool loseLock = false;

			// 条件1: プレイヤーが擬態した
			if (player_ && player_->IsMimicking())
				loseLock = true;

			// 条件2: 射程距離から大幅に外れた（1.5倍の距離まで猶予）
			Vector3 diff = playerPos - position_;
			float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
			if (dist > kDetectRange * 1.5f)
				loseLock = true;

			if (loseLock) {
				state_ = SensorState::Idle;
				object_->SetColor({1.0f, 1.0f, 1.0f, 1.0f}); // 通常色へ
			} else if (stateTimer_ >= kLockOnTime) {
				// 一定時間捉え続けたらチャージ（発射準備）へ
				state_ = SensorState::Charging;
				stateTimer_ = 0;
			}
		}
		break;

	case SensorState::Charging:
		// 【チャージ】攻撃直前！狙いを固定し、赤い激しい点滅で警告
		if (player_ && player_->IsMimicking()) {
			state_ = SensorState::Idle; // チャージ中に化けられたら見失う
			object_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		}
		{
			// sin波を使って高速点滅を表現
			float blink = std::sin(stateTimer_ * 50.0f) * 0.5f + 0.5f;
			DebugRenderer::GetInstance()->AddLine(eyePos, eyePos + lockOnDir_ * kDetectRange, {1.0f, 0.0f, 0.0f, blink});
			object_->SetColor({1.0f, 0.0f, 0.0f, 1.0f}); // 攻撃色：赤
		}
		if (stateTimer_ >= kChargeTime) {
			state_ = SensorState::Firing;
			stateTimer_ = 0;
		}
		break;

	case SensorState::Firing:
		// 【発射中】太いビーム（複数のデバッグラインの重なり）で描画
		for (int i = 0; i < 3; ++i) {
			DebugRenderer::GetInstance()->AddLine(eyePos, eyePos + lockOnDir_ * kDetectRange, {1.0f, 0.6f, 0.6f, 1.0f});
		}
		if (stateTimer_ >= kFireDuration) {
			state_ = SensorState::Cooldown;
			stateTimer_ = 0;
			object_->SetColor({0.1f, 0.1f, 0.1f, 1.0f}); // 発射直後は消灯
		}
		break;

	case SensorState::Cooldown:
		// 【再装填】しばらく待機してから、再び「Idle」に戻って首振りを再開
		if (stateTimer_ >= kCooldownTime) {
			state_ = SensorState::Idle;
			object_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
		}
		break;
	}

	// 3. 見た目の回転：現在の視線（索敵中なら振り向き、発見後ならプレイヤー）にモデルを向ける
	Vector3 lookDir = (state_ == SensorState::Idle) ? forwardDir_ : lockOnDir_;
	float yaw = std::atan2(lookDir.x, lookDir.z);
	object_->SetRotate({0.0f, yaw, 0.0f});

	// 座標と行列の最終更新
	object_->SetTranslate(position_);
}

// --- 視界の判定ロジック（初めて発見する際のみ使用） ---
bool ProminenceSensor::CanSeePlayer(const Vector3& playerPos) {
	// 1. 射程距離チェック
	Vector3 diff = playerPos - position_;
	float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
	if (distSq > kDetectRange * kDetectRange)
		return false;

	// 2. 擬態チェック（擬態中はセンサーに引っかからない）
	if (player_ && player_->IsMimicking())
		return false;

	// 3. 視野角（FOV）チェック：正面方向から一定角度(kViewHalfAngle)以内にいるか
	Vector3 dirToPlayer = Vector3::Normalized(diff);
	// 内積（Dot Product）で「センサーが向いている方向」と「プレイヤーへの方向」の合致度を計算
	float dot = dirToPlayer.x * forwardDir_.x + dirToPlayer.y * forwardDir_.y + dirToPlayer.z * forwardDir_.z;

	// 合致度(cos値)が閾値より小さければ、視界の外側にいると判断して無視
	if (dot < std::cos(kViewHalfAngle))
		return false;

	return true;
}

Vector3 ProminenceSensor::GetBeamOrigin() const {
	Vector3 eyePos = position_;
	eyePos.y += 0.6f;
	return eyePos;
}

void ProminenceSensor::Draw() {
	if (isDead_) {
		DrawDeathAnimation();
		return;
	}

	object_->Update();
	if (object_)
		object_->Draw();
}