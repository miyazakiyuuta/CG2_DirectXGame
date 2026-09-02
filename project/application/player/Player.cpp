#include "player/Player.h"

#include "bullet/BulletManager.h"
#include "input/ActionInput.h"
#include "math/CatmullRomSpline.h"
#include "3d/ModelManager.h"
#include "audio/SoundManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 被弾音。撃破音より控えめにして、爆発と重なっても濁らないようにする
	const std::string kDamageSound = "resources/sounds/damage.mp3";
	constexpr float kDamageVolume = 0.6f;
}

void Player::Initialize(ActionInput* actionInput) {
	actionInput_ = actionInput;

	// HPは満タンから始める(リトライはシーンごと作り直すため、ここが唯一の初期化点)
	hp_ = maxHp_;
	invincibleTimer_ = 0.0f;

	// 仮モデル(読み込み済みなら早期returnするので重複ロードにはならない)
	ModelManager::GetInstance()->LoadModel("sphere.obj");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("sphere.obj");
	object3d_->SetScale({ 0.5f, 0.5f, 0.5f });

	// 被弾音はSoundManagerのキャッシュと共有する
	damageSound_ = SoundManager::GetInstance()->LoadFile(kDamageSound);
}

void Player::Update(float deltaTime) {
	// 入力→オフセット移動(アクション層経由。DIK_やXInputはここでは見ない)
	offset_.x += actionInput_->GetMoveX() * moveSpeed_ * deltaTime;
	offset_.y += actionInput_->GetMoveY() * moveSpeed_ * deltaTime;
	// 可動範囲はオフセットのクランプで制限する
	offset_.x = std::clamp(offset_.x, -offsetLimit_.x, offsetLimit_.x);
	offset_.y = std::clamp(offset_.y, -offsetLimit_.y, offsetLimit_.y);

	if (rail_ && rail_->GetTotalLength() > 0.0f) {
		// レール座標系(前/右/上)を接線とワールド上方向から作る
		Vector3 forward, right, up;
		ComputeRailFrame(railDistance_, forward, right, up);
		forward_ = forward;

		// ワールド位置 = レール基準点 + 右×x + 上×y
		worldPosition_ = rail_->GetPositionByDistance(railDistance_) + right * offset_.x + up * offset_.y;

		// 銃口は自機の少し前。自機モデル(半径0.5)の内側から弾が湧いて見えるのを避ける
		muzzlePosition_ = worldPosition_ + forward * muzzleForwardOffset_;

		// 狙点: レール上のaimDistance_先の点に、自機オフセットをaimConvergence_の割合だけ足す。
		// 右/上はその地点の接線から作り直す(自機地点の軸を流用すると、カーブでは狙点が
		// レールから外れて画面外を向く)。aimConvergence_=1なら接線と平行、0なら中心線へ収束する
		Vector3 aimForward, aimRight, aimUp;
		ComputeRailFrame(railDistance_ + aimDistance_, aimForward, aimRight, aimUp);
		aimPoint_ = rail_->GetPositionByDistance(railDistance_ + aimDistance_) +
			aimRight * (offset_.x * aimConvergence_) + aimUp * (offset_.y * aimConvergence_);

		// 向きはレールカメラと同じ「接線→オイラー角」の逆算(回転規約: 行ベクトル×Rx→Ry→Rz)
		float yaw = std::atan2(forward.x, forward.z);
		float pitch = std::atan2(-forward.y, std::sqrt(forward.x * forward.x + forward.z * forward.z));
		object3d_->SetRotate({ pitch, yaw, 0.0f });
	} else {
		// レール未設定(stage.jsonにrailが無い等)でも入力確認だけはできるようにする
		worldPosition_ = { offset_.x, offset_.y, 0.0f };
		forward_ = { 0.0f, 0.0f, 1.0f };
		muzzlePosition_ = worldPosition_ + forward_ * muzzleForwardOffset_;
		aimPoint_ = worldPosition_ + forward_ * aimDistance_;
	}

	// ステージコライダーとの接触判定。ワールド位置が確定した後に行う。
	// 規模が小さいので総当たり(空間分割は敵・弾が増えてから検討する)
	hitCount_ = 0;
	if (stageColliders_) {
		for (const AABB& aabb : *stageColliders_) {
			if (IsCollision(aabb, worldPosition_, collisionRadius_)) {
				++hitCount_;
			}
		}
	}
	isHit_ = hitCount_ > 0;

	// 無敵時間の消化。先に減らしてから判定するので、被弾した次のフレームから点滅が始まる
	if (invincibleTimer_ > 0.0f) {
		invincibleTimer_ = (std::max)(invincibleTimer_ - deltaTime, 0.0f);
	}

	// 地形との接触ダメージ(A-5)。接触状態の所有者がPlayer自身なのでここで適用する。
	// 敵との接触はEnemySpawnerが判定し、シーンがTakeDamageを呼ぶ(無敵時間の管理はどちらもここ)
	if (isHit_) {
		TakeDamage(1);
	}

	// 射撃。銃口と狙点が確定した後に行う(発射方向がこの2点から決まるため)
	UpdateShooting(deltaTime);

	// 「エディタで置いたコライダーがゲームの当たり判定として効いている」ことを
	// 画面上で分かるようにする(接触中は赤)
	object3d_->SetColor(isHit_ ? Vector4{ 1.0f, 0.3f, 0.3f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });

	object3d_->SetTranslate(worldPosition_);
	object3d_->Update(deltaTime);
}

void Player::ComputeRailFrame(float distance, Vector3& forward, Vector3& right, Vector3& up) const {
	forward = rail_->GetTangentByDistance(distance);
	right = Vector3::Cross({ 0.0f, 1.0f, 0.0f }, forward);
	if (right.LengthSquared() < 1.0e-6f) {
		// 接線がほぼ真上/真下を向く縮退(外積が潰れる)。暫定でワールドXを右とする
		right = { 1.0f, 0.0f, 0.0f };
	}
	right.Normalize();
	up = Vector3::Cross(forward, right); // 直交する2軸の外積なので正規化済み
}

void Player::UpdateShooting(float deltaTime) {
	// クールタイムは撃てない間も消化する(押しっぱなしで一定間隔の連射になる)
	shootCooldown_ -= deltaTime;

	if (!bulletManager_ || !actionInput_) {
		return;
	}
	// IsTriggerではなくIsPress。RT(アナログトリガー)には瞬間判定が無いため、
	// 押しっぱなし判定+クールタイムでないとパッドのRT射撃が成立しない(ActionInput.cpp参照)
	if (!actionInput_->IsPress(ActionInput::Action::Shoot) || shootCooldown_ > 0.0f) {
		return;
	}
	shootCooldown_ = shootInterval_;

	// 発射方向は「銃口→狙点」。レール終端では狙点が手前にクランプされて方向が
	// 縮退・反転しうるので、その場合は素直に前方(接線)へ撃つ
	Vector3 direction = aimPoint_ - muzzlePosition_;
	if (direction.LengthSquared() < 1.0e-6f || Vector3::Dot(direction, forward_) <= 0.0f) {
		direction = forward_;
	}
	bulletManager_->Fire(muzzlePosition_, direction);
}

void Player::TakeDamage(int amount) {
	// 死亡後の減算と、無敵中の連続被弾は無視する。
	// 「誰がダメージを与えたか」に関係なくここ1箇所で弾くので、
	// 呼び出す側(地形・敵)は無敵時間を意識しなくてよい
	if (IsDead() || IsInvincible()) {
		return;
	}
	hp_ = (std::max)(hp_ - amount, 0);
	invincibleTimer_ = invincibleDuration_;

	// 被弾音。早期returnの後に置いてあるので、地形に触れ続けても
	// 無敵が切れるまで鳴らない(ダメージが入った瞬間とだけ一致する)
	if (damageSound_) {
		const SoundManager::SoundHandle handle = SoundManager::GetInstance()->PlayWave(
			damageSound_, false, SoundManager::SoundCategory::SE);
		SoundManager::GetInstance()->SetVolume(handle, kDamageVolume);
	}
}

void Player::Draw() {
	// 無敵中は点滅させる。
	// なぜ描画スキップか: Object3dは不透明PSOなのでαを下げても半透明にならず、
	// 色を変える方法は「地形接触=赤」と見分けが付かなくなるため
	if (IsInvincible() && std::fmod(invincibleTimer_, blinkInterval_) < blinkInterval_ * 0.5f) {
		return;
	}
	object3d_->Draw();
}

void Player::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Player");
	ImGui::Text("Offset: (%.2f, %.2f)", offset_.x, offset_.y);
	ImGui::Text("World: (%.1f, %.1f, %.1f)", worldPosition_.x, worldPosition_.y, worldPosition_.z);
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.0f, 50.0f, "%.1f m/s");
	ImGui::DragFloat2("Offset Limit", &offsetLimit_.x, 0.1f, 0.0f, 20.0f);

	// HPと無敵時間(A-5)。ダメージ源は地形接触と敵接触の2つ
	ImGui::SeparatorText("Status");
	ImGui::Text("HP: %d / %d %s", hp_, maxHp_, IsDead() ? "(DEAD)" : "");
	if (ImGui::DragInt("Max HP", &maxHp_, 0.1f, 1, 20)) {
		hp_ = (std::min)(hp_, maxHp_); // 上限を下げたときに現在HPが上限を超えたままにならないようにする
	}
	ImGui::DragFloat("Invincible", &invincibleDuration_, 0.05f, 0.0f, 5.0f, "%.2f s");
	ImGui::Text("Invincible: %s (%.2f s)", IsInvincible() ? "YES" : "no", invincibleTimer_);
	// 動作確認用。無敵時間を無視して1回分減らす
	if (ImGui::Button("Damage")) {
		invincibleTimer_ = 0.0f;
		TakeDamage(1);
	}
	ImGui::SameLine();
	if (ImGui::Button("Heal")) {
		hp_ = maxHp_;
		invincibleTimer_ = 0.0f;
	}

	// ステージコライダーとの接触状態(stage.jsonのcolliderがゲームに効いている証拠)
	ImGui::SeparatorText("Collision");
	ImGui::DragFloat("Radius", &collisionRadius_, 0.05f, 0.0f, 10.0f, "%.2f m");
	const size_t colliderCount = stageColliders_ ? stageColliders_->size() : 0;
	ImGui::Text("Stage Colliders: %zu", colliderCount);
	if (isHit_) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Hit: YES (%d)", hitCount_);
	} else {
		ImGui::Text("Hit: no");
	}

	// 射撃と照準。aimConvergenceは 1.0=接線と平行に飛ぶ / 0.0=レール中心線へ完全収束
	ImGui::SeparatorText("Shooting");
	ImGui::DragFloat("Fire Interval", &shootInterval_, 0.01f, 0.02f, 1.0f, "%.2f s");
	ImGui::DragFloat("Aim Distance", &aimDistance_, 0.5f, 1.0f, 200.0f, "%.1f m");
	ImGui::DragFloat("Aim Convergence", &aimConvergence_, 0.01f, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Muzzle Offset", &muzzleForwardOffset_, 0.05f, 0.0f, 10.0f, "%.2f m");
	ImGui::Text("Aim: (%.1f, %.1f, %.1f)", aimPoint_.x, aimPoint_.y, aimPoint_.z);

	// Boostは未実装(定義のみ)。マッピング確認用に押下状態だけ見せる
	ImGui::SeparatorText("Actions (mapping check)");
	ImGui::Text("Shoot: %s", actionInput_->IsPress(ActionInput::Action::Shoot) ? "PRESS" : "-");
	ImGui::Text("Boost: %s", actionInput_->IsPress(ActionInput::Action::Boost) ? "PRESS" : "-");
	ImGui::End();
#endif
}

void Player::SetCamera(Camera* camera) {
	object3d_->SetCamera(camera);
}

Player::Player() = default;

Player::~Player() = default;
