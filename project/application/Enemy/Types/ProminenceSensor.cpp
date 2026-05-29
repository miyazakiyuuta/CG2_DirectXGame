#include "ProminenceSensor.h"
#include "../../../application/Player.h"
#include "../../../engine/3d/Object3d.h"
#include "../../../engine/3d/ModelManager.h"
#include <algorithm>
#include <cmath>
#include "../../../engine/math/Matrix4x4.h"

// --- コンストラクタ：変数の初期化 ---
ProminenceSensor::ProminenceSensor() : state_(SensorState::Idle), stateTimer_(0.0f), lockOnDir_({0, 0, 1}), forwardDir_({0, 0, 1}), baseForwardDir_({0, 0, 1}), searchTimer_(0.0f) {}

ProminenceSensor::~ProminenceSensor() = default;

// --- 初期化：見た目や物理挙動のセットアップ ---
void ProminenceSensor::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	// 3Dオブジェクトの基本設定（センサー本体）
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

	// --- ビーム用Object3dの生成 ---
	// beam.objはY軸方向に伸びる円柱（原点が底面中心）
	beamObject_ = std::make_unique<Object3d>();
	beamObject_->Initialize(common);
	beamObject_->SetModel("beam.obj");
	beamObject_->SetCamera(camera);
	// ライティングを無効化して自己発光風にする
	beamObject_->SetEnableLighting(false);
	// 初期状態では非表示
	beamVisible_ = false;
}

void ProminenceSensor::SetRotation(const Vector3& rot) {
	// 回転行列を作成し、基準となる前方ベクトル（Z+方向）を回転させる
	Matrix4x4 matRot = Matrix4x4::Rotate(rot);
	baseForwardDir_ = matRot.Transform({0.0f, 0.0f, 1.0f});
	
	// 索敵中の現在の方向も基準に合わせる
	forwardDir_ = baseForwardDir_;

	// 見た目（モデル）の回転も更新
	if (object_) {
		object_->SetRotate(rot);
	}
}

// --- ビームObject3dの座標・回転・スケールを計算して適用する ---
// origin: ビームの射出位置（センサーの目の位置）
// direction: ビームの方向（正規化済みベクトル）
// length: ビームの長さ
void ProminenceSensor::UpdateBeamTransform(const Vector3& origin, const Vector3& direction, float length, float thickness) {
	// OBJモデルはY軸方向（y=0→y=1）に伸びる円柱なので
	// ビーム方向ベクトルに向けるための回転を計算する

	// Y軸回転（yaw）：XZ平面での方向
	float yaw = std::atan2(direction.x, direction.z);

	// X軸回転（pitch）：仰角/俯角
	// 左手系でY軸(上)をZ軸+(奥)へ倒すには、X軸周りに+90度(π/2)回転させる。
	// 仰角/俯角はそこから引くことで調整する。
	float horizontalDist = std::sqrt(direction.x * direction.x + direction.z * direction.z);
	float pitch = (3.14159265f / 2.0f) - std::atan2(direction.y, horizontalDist);

	// Object3dに適用
	beamObject_->SetTranslate(origin);
	beamObject_->SetRotate({pitch, yaw, 0.0f});
	// X,Z = ビームの太さ
	// Y = ビームの長さ
	beamObject_->SetScale({thickness, length, thickness});
}

// --- 更新処理：ステートマシンと索敵AI ---
void ProminenceSensor::Update(float deltaTime, const Vector3& playerPos) {
	if (isDead_) {
		UpdateDeathAnimation(deltaTime);
		beamVisible_ = false; // 死亡時はビームを消す
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

		// 索敵中のビームを表示（青色・半透明）
		beamVisible_ = true;
		beamColor_ = {0.2f, 0.6f, 1.0f, 0.4f};
		UpdateBeamTransform(eyePos, forwardDir_, kDetectRange, 0.15f);

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

		// 追尾中のビームを表示（赤色）
		beamVisible_ = true;
		beamColor_ = {1.0f, 0.0f, 0.0f, 0.6f};
		UpdateBeamTransform(eyePos, lockOnDir_, kDetectRange, 0.15f);

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
			beamVisible_ = true;
			beamColor_ = {1.0f, 0.0f, 0.0f, blink};
			UpdateBeamTransform(eyePos, lockOnDir_, kDetectRange, 0.15f);
			object_->SetColor({1.0f, 0.0f, 0.0f, 1.0f}); // 攻撃色：赤
		}
		if (stateTimer_ >= kChargeTime) {
			state_ = SensorState::Firing;
			stateTimer_ = 0;
		}
		break;

	case SensorState::Firing:
		// 【発射中】太いビームをOBJモデルで描画
		beamVisible_ = true;
		beamColor_ = {1.0f, 0.6f, 0.6f, 1.0f};
		// 発射中はビームを太くする
		UpdateBeamTransform(eyePos, lockOnDir_, kDetectRange, kBeamRadius * 2.5f);

		if (stateTimer_ >= kFireDuration) {
			state_ = SensorState::Cooldown;
			stateTimer_ = 0;
			object_->SetColor({0.1f, 0.1f, 0.1f, 1.0f}); // 発射直後は消灯
		}
		break;

	case SensorState::Cooldown:
		// 【再装填】ビームを消して待機、再びIdleへ
		beamVisible_ = false;
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

	// センサー本体の描画
	object_->Update();
	if (object_)
		object_->Draw();

	// ビームOBJモデルの描画
	if (beamVisible_ && beamObject_) {
		beamObject_->SetColor(beamColor_);
		beamObject_->Update();
		beamObject_->Draw();
	}
}