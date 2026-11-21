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
	// 自機モデル(駆除ドローン)。全長1.0に正規化済みで、実寸 約0.98 x 0.25 x 1.0(幅/高さ/全長)、機首が-Z
	// (モデルの規格。仕様は tools/models/drone/drone_report.txt)
	const std::string kModel = "drone/drone.obj";
	// 幅を約1.08にして、以前の宇宙船(5.4×0.2)と同じ画面上の大きさ・判定半径(0.5)にそろえる。
	// 最終的な値は、床の板(幅15cm)や家具と並べて見てから決める(1単位=1cm)
	constexpr float kModelScale = 1.1f;

	// 被弾音。撃破音より控えめにして、爆発と重なっても濁らないようにする
	const std::string kDamageSound = "resources/sounds/damage.mp3";
	constexpr float kDamageVolume = 0.6f;

	// チャージ満タンの音とロック音。どちらも合成で作った仮の音なので、
	// 同じ名前のファイルを置き換えれば差し替わる(mp3にするなら拡張子もここで直す)
	const std::string kChargeReadySound = "resources/sounds/charge_ready.wav";
	constexpr float kChargeReadyVolume = 0.6f;
	const std::string kLockOnSound = "resources/sounds/lockon.wav";
	constexpr float kLockOnVolume = 0.7f;

	// 効果音を1回鳴らす(音声ファイルが読めていなければ何もしない=ゲームは止めない)
	void PlaySe(const std::shared_ptr<const SoundData>& sound, float volume) {
		if (!sound) {
			return;
		}
		const SoundManager::SoundHandle handle =
			SoundManager::GetInstance()->PlayWave(sound, false, SoundManager::SoundCategory::SE);
		SoundManager::GetInstance()->SetVolume(handle, volume);
	}
}

const std::string& Player::ModelPath() {
	return kModel;
}

float Player::ModelScale() {
	return kModelScale;
}

Matrix4x4 Player::ModelCorrection() {
	// モデルの規格では機首が-Z、ゲーム(レール・カメラ・墜落演出)は機首を+Zとして向きを計算する。
	// Y軸まわりに半回転させて、モデルの機首を+Zへ向け直す
	return Matrix4x4::RotateY(std::numbers::pi_v<float>);
}

void Player::Initialize(ActionInput* actionInput) {
	actionInput_ = actionInput;

	ModelManager::GetInstance()->LoadModel(kModel);

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel(kModel);
	object3d_->SetScale({ kModelScale, kModelScale, kModelScale });

	// 被弾音・チャージ満タンの音・ロック音はSoundManagerのキャッシュと共有する
	damageSound_ = SoundManager::GetInstance()->LoadFile(kDamageSound);
	chargeReadySound_ = SoundManager::GetInstance()->LoadFile(kChargeReadySound);
	lockOnSound_ = SoundManager::GetInstance()->LoadFile(kLockOnSound);

	// 状態の初期化はReset()に集約
	Reset();
}

void Player::Update(float deltaTime) {
	// 入力をオフセット移動に。止められている間(イントロ中など)は入力を読まない。
	// deltaTimeを0にして止めないのは、無敵時間やモデルの更新まで止まってしまうため
	// (「時間を止める」と「操作させない」は別の意味なので、許可フラグで分ける)
	if (moveEnabled_) {
		offset_.x += actionInput_->GetMoveX() * moveSpeed_ * deltaTime;
		offset_.y += actionInput_->GetMoveY() * moveSpeed_ * deltaTime;
	}
	// 可動範囲をオフセットのクランプで制限する
	offset_.x = std::clamp(offset_.x, -offsetLimit_.x, offsetLimit_.x);
	offset_.y = std::clamp(offset_.y, -offsetLimit_.y, offsetLimit_.y);

	if (rail_ && rail_->GetTotalLength() > 0.0f) {
		// レール座標系(前/右/上)を接線とワールド上方向から作る
		Vector3 forward;
		Vector3 right;
		Vector3 up;
		ComputeRailFrame(railDistance_, forward, right, up);
		forward_ = forward;

		// ワールド位置 = レール基準点 + 右×x + 上×y
		worldPosition_ = rail_->GetPositionByDistance(railDistance_) + right * offset_.x + up * offset_.y;

		// 銃口は自機の少し前。自機モデル(半径0.5)の内側から弾が湧いて見えるのを避ける
		muzzlePosition_ = worldPosition_ + forward * muzzleForwardOffset_;

		// 狙点: レール上のaimDistance_先の点に、自機オフセットをaimConvergence_の割合だけ足す。
		// 右/上はその地点の接線から作り直す(自機地点の軸を流用すると、カーブでは狙点が
		// レールから外れて画面外を向く)。aimConvergence_=1なら接線と平行、0なら中心線へ収束、1より大きいと外側へ広がる
		Vector3 aimForward, aimRight, aimUp;
		ComputeRailFrame(railDistance_ + aimDistance_, aimForward, aimRight, aimUp);
		aimPoint_ = rail_->GetPositionByDistance(railDistance_ + aimDistance_) +
			aimRight * (offset_.x * aimConvergence_) + aimUp * (offset_.y * aimConvergence_);

		// 飛ぶ向きはレールカメラと同じ「接線→オイラー角」の逆算(回転規約: 行ベクトル×Rx→Ry→Rz)。
		// 機首を+Zとしたときの向きで、GetRotateが返す(墜落演出が「被弾した瞬間の機首の向き」として使う)
		float yaw = std::atan2(forward.x, forward.z);
		float pitch = std::atan2(-forward.y, std::sqrt(forward.x * forward.x + forward.z * forward.z));
		rotate_ = { pitch, yaw, 0.0f };
	} else {
		// レール未設定(stage.jsonにrailが無い等)でも入力確認だけはできるようにする
		worldPosition_ = { offset_.x, offset_.y, 0.0f };
		forward_ = { 0.0f, 0.0f, 1.0f };
		muzzlePosition_ = worldPosition_ + forward_ * muzzleForwardOffset_;
		aimPoint_ = worldPosition_ + forward_ * aimDistance_;
		rotate_ = { 0.0f, 0.0f, 0.0f }; // +Z向き
	}

	// モデルの向き = 先にモデルの機首を+Zへ向け直し(ModelCorrection)、そのあと飛ぶ向きへ回す。
	// 行ベクトル規約なので、左に置いた行列ほど先(モデル自身の座標)で効く
	object3d_->SetRotate(Matrix4x4::ToEuler(ModelCorrection() * Matrix4x4::Rotate(rotate_)));

	// ステージコライダーとの接触判定。ワールド位置が確定した後に行う。
	// 規模が小さいので総当たり(空間分割は敵・弾が増えてから検討する)
	hitCount_ = 0;
	// 当たった地形のうち、自機に最も近い点(=ぶつかった場所)。複数に触れていても最初の1つで代表させる
	Vector3 terrainHitPoint = worldPosition_;
	if (stageColliders_) {
		for (const AABB& aabb : *stageColliders_) {
			if (IsCollision(aabb, worldPosition_, collisionRadius_)) {
				if (hitCount_ == 0) {
					// 箱の中へ各軸を丸めた点が、箱の上で自機に最も近い点になる
					terrainHitPoint = {
						std::clamp(worldPosition_.x, aabb.min.x, aabb.max.x),
						std::clamp(worldPosition_.y, aabb.min.y, aabb.max.y),
						std::clamp(worldPosition_.z, aabb.min.z, aabb.max.z) };
				}
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
		TakeDamage(1, terrainHitPoint);
	}

	// 敵弾との当たり判定。無敵時間を消化した後に行うので、
	// 被弾直後の無敵中に飛んできた弾はダメージにならない(弾自体は消える)
	UpdateEnemyBulletCollision();

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
	// クールタイムは撃てない間も消化する
	shootCooldown_ -= deltaTime;

	if (!actionInput_) {
		return;
	}

	// 押した瞬間・離した瞬間を、前フレームの押下状態との比較で求める(エッジ検出)。
	// 押下状態の記録は撃てない間も続ける: 止めると、押したままイントロが明けたフレームを
	// 「押した瞬間」と取り違え、押し直していないのに1発出てしまう
	const bool pressed = actionInput_->IsPress(ActionInput::Action::Shoot);
	const bool justPressed = pressed && !wasShootPressed_;
	const bool justReleased = !pressed && wasShootPressed_;
	wasShootPressed_ = pressed;

	// 射撃を止められている間(エディタのEditモード・イントロ中)は撃たず、チャージも溜めない。
	// 溜まっていた分とロックも捨てる(撃てない間を溜めていた時間に数えない)
	if (!shootEnabled_) {
		chargeTimer_ = 0.0f;
		lockTargetId_ = 0;
		return;
	}

	// 押した瞬間に通常弾を1発。押しっぱなしでは連射しない(連打で撃つ)。
	// 長押しはチャージに使うため、押している間ずっと撃つと「溜めながら撃ち続ける」ことになる
	if (justPressed && shootCooldown_ <= 0.0f && bulletManager_) {
		shootCooldown_ = shootInterval_;
		bulletManager_->Fire(muzzlePosition_, ComputeShotDirection());
	}

	// チャージは押している間だけ溜まり、満タンで頭打ちにする。
	// 「毎秒量×dt」で溜めるので、ヒットストップ(dt=0)の間は止まる。
	// チャージ弾が飛んでいる間は溜めない(同時に飛ぶのは1発まで)
	if (pressed) {
		if (CanCharge()) {
			const bool wasCharged = IsCharged();
			chargeTimer_ = (std::min)(chargeTimer_ + deltaTime, chargeDuration_);
			// 満タンになった瞬間だけ鳴らす(満タンのまま押している間は鳴らし直さない)
			if (!wasCharged && IsCharged()) {
				PlaySe(chargeReadySound_, kChargeReadyVolume);
			}
		}
		return;
	}

	// 離した瞬間に満タンなら、チャージ弾を撃つ。ロック中ならその敵の番号を付けて撃ち、弾が追尾する。
	// 満タン前に離した場合は何もしない: 押した瞬間の通常弾はもう出ている
	if (justReleased && IsCharged() && CanCharge()) {
		chargeBulletManager_->Fire(muzzlePosition_, ComputeShotDirection(), lockTargetId_);
	}
	chargeTimer_ = 0.0f;
	lockTargetId_ = 0; // 離したらロックも外す(次のロックは、また満タンまで溜めてから)
}

Vector3 Player::ComputeShotDirection() const {
	// 発射方向は「銃口→狙点」。レール終端では狙点が手前にクランプされて方向が
	// 縮退・反転しうるので、その場合は素直に前方(接線)へ撃つ
	Vector3 direction = aimPoint_ - muzzlePosition_;
	if (direction.LengthSquared() < 1.0e-6f || Vector3::Dot(direction, forward_) <= 0.0f) {
		direction = forward_;
	}
	return direction;
}

bool Player::CanCharge() const {
	return chargeBulletManager_ && chargeBulletManager_->HasFreeSlot();
}

void Player::SetLockTarget(uint32_t enemyId) {
	// ロックしていない状態から新しくロックした瞬間だけ鳴らす(外すとき・同じ敵のままのときは鳴らさない)
	if (lockTargetId_ == 0 && enemyId != 0) {
		PlaySe(lockOnSound_, kLockOnVolume);
	}
	lockTargetId_ = enemyId;
}

float Player::GetChargeRatio() const {
	// チャージ時間が0以下なら常に満タン扱い(IsChargedと同じ結果にそろえ、0除算もしない)
	if (chargeDuration_ <= 0.0f) {
		return 1.0f;
	}
	return std::clamp(chargeTimer_ / chargeDuration_, 0.0f, 1.0f);
}

void Player::UpdateEnemyBulletCollision() {
	if (!enemyBulletManager_) {
		return; // 敵弾が存在しない(エディタ単体での確認時など)
	}
	// 被弾を止められている間(Editモード・イントロ・ゲームオーバー演出)は、自機がそこに居ない扱いにして弾を素通りさせる。
	// 消してしまうと、ゲームオーバー演出の最中に飛んできた弾が、もう誰も居ない場所で急に消えて見える
	if (!damageEnabled_) {
		return;
	}

	const std::vector<Bullet>& bullets = enemyBulletManager_->GetBullets();
	const float bulletRadius = enemyBulletManager_->GetRadius();

	// 総当たり。敵弾も最大64発なので空間分割は要らない(自弾→敵の判定と同じ規模)
	for (size_t i = 0; i < bullets.size(); ++i) {
		if (!bullets[i].active) {
			continue;
		}
		if (!IsCollision(worldPosition_, collisionRadius_, bullets[i].position, bulletRadius)) {
			continue;
		}

		// 当たった弾の位置を、消す前に控えておく(どこから当たったかとしてTakeDamageへ渡す)
		const Vector3 bulletPosition = bullets[i].position;
		// 当たった弾は無敵中でも消す。残すと無敵が切れた瞬間に同じ弾で食らい直し、
		// 「避けたのに当たった」ように見える
		enemyBulletManager_->Kill(i);
		// 複数発が同じフレームに当たってもTakeDamage側の無敵で1回にまとめられる。
		// 判定はTakeDamageの「前」に行う(成功すると無敵になり、後で判定すると常にfalseになるため)
		if (CanTakeDamage()) {
			++enemyBulletHitCount_;
		}
		// ダメージ量は弾のProfileが持つ(既定は1。自弾→敵と同じく弾のデータで決める)
		TakeDamage(enemyBulletManager_->GetDamage(), bulletPosition);
	}
}

void Player::TakeDamage(int amount) {
	ApplyDamage(amount, nullptr);
}

void Player::TakeDamage(int amount, const Vector3& sourcePosition) {
	ApplyDamage(amount, &sourcePosition);
}

bool Player::TryGetLastHitSource(Vector3& outPosition) const {
	if (!hasLastHitSource_) {
		return false;
	}
	outPosition = lastHitSource_;
	return true;
}

void Player::ApplyDamage(int amount, const Vector3* sourcePosition) {
	// 死亡後の減算と、無敵中の連続被弾、外から被弾を無効にされている間(エディタのEditモード)は無視する。
	// 「誰がダメージを与えたか」に関係なくここ1箇所で弾くので、
	// 呼び出す側(地形・敵)は無敵時間やモードを意識しなくてよい
	if (!CanTakeDamage()) {
		return;
	}
	hp_ = (std::max)(hp_ - amount, 0);
	invincibleTimer_ = invincibleDuration_;

	// どこから当たったかは、実際にダメージが入ったときだけ書き換える(無敵で弾かれた当たりでは上書きしない)。
	// 分からない被弾なら「分からない」で上書きする: 前の被弾の位置が残ると、関係ない場所から煙が出てしまう
	hasLastHitSource_ = sourcePosition != nullptr;
	if (sourcePosition) {
		lastHitSource_ = *sourcePosition;
	}

	// 被弾音。早期returnの後に置いてあるので、地形に触れ続けても
	// 無敵が切れるまで鳴らない(ダメージが入った瞬間とだけ一致する)
	PlaySe(damageSound_, kDamageVolume);
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
	// 敵弾で何回食らったか(敵の攻撃が自機に効いていることの確認用)
	ImGui::Text("Enemy Bullet Hits: %d", enemyBulletHitCount_);
	if (isHit_) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Hit: YES (%d)", hitCount_);
	} else {
		ImGui::Text("Hit: no");
	}

	// 射撃と照準。aimConvergenceは 1.0=接線と平行に飛ぶ / 0.0=レール中心線へ完全収束 / 1より大きい=外側へ広がる
	// (大きいほど画面上でレティクルが自機の近くまで動く。約3.5で自機と同じ動きになる)
	ImGui::SeparatorText("Shooting");
	ImGui::DragFloat("Min Fire Interval", &shootInterval_, 0.01f, 0.02f, 1.0f, "%.2f s");
	ImGui::DragFloat("Aim Distance", &aimDistance_, 0.5f, 1.0f, 200.0f, "%.1f m");
	ImGui::DragFloat("Aim Convergence", &aimConvergence_, 0.01f, 0.0f, 4.0f, "%.2f");
	ImGui::DragFloat("Muzzle Offset", &muzzleForwardOffset_, 0.05f, 0.0f, 10.0f, "%.2f m");
	ImGui::Text("Aim: (%.1f, %.1f, %.1f)", aimPoint_.x, aimPoint_.y, aimPoint_.z);

	// チャージ(長押し)。満タンで離すとチャージ弾が出る(弾の速さ・ダメージ・爆発は"Charge Bullet"ウィンドウ)
	ImGui::SeparatorText("Charge");
	ImGui::DragFloat("Charge Time", &chargeDuration_, 0.01f, 0.1f, 3.0f, "%.2f s");
	ImGui::ProgressBar(GetChargeRatio(), ImVec2(-1.0f, 0.0f), IsCharged() ? "CHARGED" : nullptr);
	// in flightの間は溜まらない(同時に飛ぶのは1発まで)
	ImGui::Text("Charge Shot: %s", CanCharge() ? "ready" : "in flight");
	// ロックオン。チャージ中は照準からLock Assistの角度以内にいる敵へ照準が引き寄せられ、満タンでロックする
	// (0度=重ねたときだけ / 30度前後=画面のどこにいても自動でロック)
	ImGui::SliderAngle("Lock Assist", &lockAssistAngle_, 0.0f, 45.0f);
	ImGui::DragFloat("Lock Range", &lockRange_, 1.0f, 1.0f, 300.0f, "%.0f m");
	if (lockTargetId_ != 0) {
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.2f, 1.0f), "Lock: enemy #%u", lockTargetId_);
	} else {
		ImGui::Text("Lock: none");
	}

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

const Vector3& Player::GetRotate() const {
	// モデルの向き(補正込み)ではなく、飛ぶ向き(機首を+Zとしたときの向き)を返す
	return rotate_;
}

void Player::Reset() {
	// ImGui で調整する設定値は戻さない(Stop のたびに調整が消えると、遊びながら調整する意味がなくなるため)
	// メンバを足したら、状態か設定かを決め、状態ならここに足すこと
	offset_ = { 0.0f, 0.0f };
	hp_ = maxHp_;
	invincibleTimer_ = 0.0f;
	hasLastHitSource_ = false; // 前のプレイの被弾位置を持ち越さない
	enemyBulletHitCount_ = 0; // 敵弾で何回食らったかのカウンタをリセット(ImGui表示用)
	shootCooldown_ = 0.0f;
	chargeTimer_ = 0.0f;
	lockTargetId_ = 0;
	// wasShootPressed_は戻さない: ゲームの状態ではなく「物理的なボタンの前回値」なので、
	// 押したままResetしたときに戻すと、押し直していないのに「押した瞬間」と取り違える
}

Player::Player() = default;

Player::~Player() = default;
