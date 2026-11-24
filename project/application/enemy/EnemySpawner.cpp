#include "enemy/EnemySpawner.h"

#include "bullet/BulletManager.h"
#include "enemy/Enemy.h"
#include "enemy/RammerEnemy.h"
#include "enemy/ShooterEnemy.h"
#include "stage/StageData.h"
#include "math/Collision.h"
#include "debug/DebugRenderer.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "audio/SoundManager.h"
#include "utility/Random.h"
#include "math/Vector4.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 敵の種別 → モデルの対応表。データ(stage.json)にはモデルパスを持たせず種別だけを持たせ、
	// 「どの見た目で出すか」はコード側のこの1箇所で決める(種別追加はここに足すだけ)
	// 射撃型・接近型はまだ仮のシロアリ(白い箱)で、大きさ(ScaleOf)だけで見分ける。
	// 白いのは、HPの残量を色で見せる(Enemy::ApplyHpColor: 満タン=白→赤)のに一番読み取りやすいため。
	// 女王は専用のモデル(頭が-Z・原点は体の重心・全長1の規格)。色はパレット画像に掛け算で効くので、
	// HPの減りは同じく赤みとして見える。
	// 変数名の frog/sword はstage.jsonに書かれた種別名(データのキー)で、見た目とは無関係
	const std::string kFrogModel = "termite/termite.obj";  // 射撃型
	const std::string kSwordModel = "termite/termite.obj"; // 接近型
	const std::string kQueenModel = "termite_queen/termite_queen.obj"; // 女王(倒すとクリア)

	// 撃破エフェクトのグループ名。configとテクスチャの登録はシーン側で行う
	const std::string kDefeatEffect = "enemyExplosion";
	// 全滅(クリア演出)で消えるときの煙のグループ名。登録はシーン側で行う(撃破エフェクトと同じ分担)
	const std::string kExterminateEffect = "enemyExterminate";
	// 全滅で落ちるときの色。色が抜けた暗い灰色にして「動かなくなった」ことを見せる
	const Vector4 kExterminatedColor = { 0.25f, 0.25f, 0.25f, 1.0f };

	// 撃破音。発射音と違い同時に何発も鳴らないので、こちらは絞らない
	const std::string kDefeatSound = "resources/sounds/explosion.mp3";
	constexpr float kDefeatVolume = 0.8f;

	// 接近音。体当たり型が間合いに入った合図。同時に鳴らすのは1つまでに絞る(PlayApproachSound)
	const std::string kApproachSound = "resources/sounds/approach.mp3";
	constexpr float kApproachVolume = 0.8f;
}

const std::string& EnemySpawner::DefeatEffectName() {
	return kDefeatEffect;
}

const std::string& EnemySpawner::ExterminateEffectName() {
	return kExterminateEffect;
}

const std::string& EnemySpawner::ModelPathOf(const std::string& enemy) {
	if (enemy == "sword") {
		return kSwordModel;
	}
	if (enemy == "queen") {
		return kQueenModel;
	}
	return kFrogModel; // 既定(未知の種別もここへ落ちるので、データ不備でも消えずに気付ける)
}

float EnemySpawner::ScaleOf(const std::string& enemy) {
	// 箱の実寸は1x1x1。幅が判定の直径(radius x 2)になる倍率にする
	// (判定の球は箱に内接する。角は少しはみ出すが、正面から見た幅と判定の幅がそろう)
	if (enemy == "sword") {
		return 4.0f; // 接近型: 幅4m(小さく速い)
	}
	if (enemy == "queen") {
		// 女王: モデルは全長1・胴の最大幅0.3108。正面から見た幅が前の箱と同じ12mになる倍率にする(全長は約39m)。
		// 終点では真正面から見るので、画面での大きさと当てやすさ(判定半径6m)が箱のときと変わらない
		return 38.6f;
	}
	return 6.0f;     // 射撃型: 幅6m
}

const std::vector<std::string>& EnemySpawner::EnemyTypes() {
	static const std::vector<std::string> kTypes = { "frog", "sword", "queen" };
	return kTypes;
}

std::unique_ptr<Enemy> EnemySpawner::CreateEnemy(const std::string& enemy) {
	// 振る舞いの違いで分岐するのはここ1か所だけ。作った後は基底のポインタで扱うので、
	// 更新処理(Update)には種別の分岐を持ち込まない
	if (enemy == "sword") {
		return std::make_unique<RammerEnemy>(); // 体当たり型
	}
	// 射撃型(frog)と女王(queen)。女王に固有の動きを作るときは、ここでQueenEnemyを返すように分ける。
	// 未知の種別も射撃型へ落とす(ModelPathOfと同じく、データ不備でも敵が消えないので気付ける)
	return std::make_unique<ShooterEnemy>();
}

bool EnemySpawner::HasGoal() const {
	return std::any_of(spawnPoints_.begin(), spawnPoints_.end(), [this](const SpawnPoint& spawnPoint) {
		return typeParams_[TypeIndexOf(spawnPoint.enemy)].isGoal;
	});
}

bool EnemySpawner::IsGoalDefeated() const {
	bool hasGoal = false;
	for (const SpawnPoint& spawnPoint : spawnPoints_) {
		if (!typeParams_[TypeIndexOf(spawnPoint.enemy)].isGoal) {
			continue;
		}
		hasGoal = true;
		if (spawnPoint.outcome != SpawnPoint::Outcome::Defeated) {
			return false; // 残っている女王がいる
		}
	}
	return hasGoal;
}

bool EnemySpawner::TryGetGoalDefeatPose(GoalPose& outPose) const {
	if (!hasGoalDefeatPose_) {
		return false;
	}
	outPose = goalDefeatPose_;
	return true;
}

bool EnemySpawner::TryGetGoalSpawnPose(GoalPose& outPose) const {
	for (const SpawnPoint& spawnPoint : spawnPoints_) {
		const EnemyTypeParams& params = typeParams_[TypeIndexOf(spawnPoint.enemy)];
		if (!params.isGoal) {
			continue;
		}
		outPose.enemy = spawnPoint.enemy;
		outPose.transform = spawnPoint.transform;
		// 大きさは発生時と同じく種別ごとの既定を使う(データ側のscaleは見ない)
		const float scale = ScaleOf(spawnPoint.enemy);
		outPose.transform.scale = { scale, scale, scale };
		outPose.center = Enemy::ForwardPoint(spawnPoint.transform.translate, spawnPoint.transform.rotate, params.hitOffset);
		outPose.radius = params.radius;
		return true;
	}
	return false;
}

bool EnemySpawner::ForceDefeatGoal() {
	bool found = false;
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (ParamsOf(*enemy).isGoal && enemy->IsInCombat()) {
			enemy->ForceDefeat(); // 撃破の処理は次のUpdateの「撃破された敵を取り除く」で行われる
			found = true;
		}
	}
	return found;
}

void EnemySpawner::FreezeAll() {
	frozen_ = true;
	playerHit_ = false; // 固まった敵はもう触れてもダメージにならない
}

void EnemySpawner::ExterminateAll() {
	frozen_ = true; // FreezeAllを通らずに呼ばれても、以降は固まった扱いにする
	// ImGuiで下限と上限を逆にされても乱数の範囲が壊れないよう、小さい方を下限にする
	const float lifeMin = (std::min)(exterminateLifeMin_, exterminateLifeMax_);
	const float lifeMax = (std::max)(exterminateLifeMin_, exterminateLifeMax_);
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (enemy->IsDying()) {
			continue;
		}
		// 消えるまでの長さを個体ごとに散らす。一斉に消えると「全滅」ではなく「表示を消した」ように見えるため
		const float duration = Random::GetFloat(lifeMin, lifeMax);
		// 少しだけ跳ねてから落ちる(力が抜けて、足場を離れる感じ)。横へも少し散らして重なりを避ける
		const Vector3 velocity = {
			Random::GetFloat(-2.0f, 2.0f), Random::GetFloat(0.0f, 3.0f), Random::GetFloat(-2.0f, 2.0f) };
		const float spin = Random::GetFloat(-6.0f, 6.0f);
		enemy->StartDying(duration, velocity, spin, kExterminatedColor);
	}
}

void EnemySpawner::UpdateFrozen(float deltaTime) {
	for (size_t i = enemies_.size(); i > 0; --i) {
		Enemy& enemy = *enemies_[i - 1];
		if (enemy.IsDying()) {
			// 落ちる(重力で速度を積み、転がりながら下へ)
			enemy.UpdateDying(deltaTime, exterminateGravity_);

			// 落ち切ったら煙にして消す。撃破・取り逃しのどちらにも記録しない(評価はプレイヤーが倒した分だけ)
			if (enemy.IsDyingFinished()) {
				ParticleManager::GetInstance()->Emit(
					kExterminateEffect, enemy.GetPosition(), static_cast<uint32_t>((std::max)(exterminateSmokeCount_, 0)));
				graveyard_.push_back(enemy.ReleaseObject());
				enemies_.erase(enemies_.begin() + static_cast<ptrdiff_t>(i - 1));
				continue;
			}
		}
		// 固まっているだけの敵は動かさない(行列の書き込みだけ行う)
		enemy.UpdateObject(deltaTime);
	}
}

size_t EnemySpawner::TypeIndexOf(const std::string& enemy) const {
	for (size_t i = 0; i < typeParams_.size(); ++i) {
		if (typeParams_[i].name == enemy) {
			return i;
		}
	}
	// 未知の種別は既定へ(ModelPathOfと同じ扱い。データ不備でも敵が消えないので気付ける)
	return 0;
}

const EnemyTypeParams& EnemySpawner::ParamsOf(const Enemy& enemy) const {
	return typeParams_[(std::min)(enemy.GetTypeIndex(), typeParams_.size() - 1)];
}

Vector3 EnemySpawner::HitCenter(const Enemy& enemy) const {
	return enemy.HitCenter(ParamsOf(enemy));
}

void EnemySpawner::RetireEnemies() {
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		graveyard_.push_back(enemy->ReleaseObject());
	}
	enemies_.clear();
	playerHit_ = false; // 敵が居なくなるので接触状態も落とす
}

void EnemySpawner::BuildFromStage(const StageData& stageData) {
	// 撃破音は初回だけ読む(BuildFromStageはReloadのたびに呼ばれるため)
	if (!defeatSound_) {
		defeatSound_ = SoundManager::GetInstance()->LoadFile(kDefeatSound);
	}
	if (!approachSound_) {
		approachSound_ = SoundManager::GetInstance()->LoadFile(kApproachSound);
	}

	// 構築し直すので、発生済みの敵も含めて作り直す(Reloadで配置が変わるため)
	spawnPoints_.clear();
	RetireEnemies();
	// 全滅(クリア演出)の状態も開始時に戻す
	frozen_ = false;
	hasGoalDefeatPose_ = false;

	for (const StageData::ObjectData& objectData : stageData.objects) {
		if (objectData.type != StageData::ObjectType::Spawn) {
			continue;
		}
		// 無効フラグはstaticと同じ意味(データは残すがゲームには出さない)
		if (objectData.disabled) {
			continue;
		}

		SpawnPoint spawnPoint;
		spawnPoint.enemy = objectData.enemy;
		spawnPoint.railDistance = objectData.railDistance;
		spawnPoint.transform = objectData.transform;
		spawnPoints_.push_back(std::move(spawnPoint));

		// 発生時に読み込むと1フレーム重くなるため、モデルは先に読んでおく
		ModelManager::GetInstance()->LoadModel(ModelPathOf(objectData.enemy));
	}
}

void EnemySpawner::UpdateBulletCollision(BulletManager* bulletManager) {
	if (!bulletManager) {
		return; // 弾の供給元が無ければ被弾しない(エディタ単体での確認時など)
	}

	const std::vector<Bullet>& bullets = bulletManager->GetBullets();
	const float bulletRadius = bulletManager->GetRadius();
	// 弾の種類ごとの違いはこの2つの値だけ(通常弾: 1ダメージ・爆発なし / チャージ弾: 大ダメージ・爆発あり)
	const int damage = bulletManager->GetDamage();
	const float explosionRadius = bulletManager->GetExplosionRadius();

	// 総当たり。敵は同時数体、弾は最大64発なので空間分割は要らない
	for (size_t bulletIndex = 0; bulletIndex < bullets.size(); ++bulletIndex) {
		const Bullet& bullet = bullets[bulletIndex];
		if (!bullet.active) {
			continue;
		}

		for (const std::unique_ptr<Enemy>& enemy : enemies_) {
			// 同フレームに撃破済み(1発で2体分減らさない)か、通過済み(離脱中の敵は撃ち落とせない)
			if (!enemy->IsInCombat()) {
				continue;
			}
			if (!IsCollision(bullet.position, bulletRadius, HitCenter(*enemy), ParamsOf(*enemy).radius)) {
				continue;
			}

			// 当たった弾は消す(貫通させない)。撃破処理はUpdate側でまとめて行う
			bulletManager->Kill(bulletIndex);
			if (explosionRadius > 0.0f) {
				Explode(bullet.position, explosionRadius, damage, *enemy);
			} else {
				enemy->ApplyDamage(damage, ParamsOf(*enemy).maxHp);
			}
			break; // この弾は消費済みなので次の弾へ
		}
	}
}

void EnemySpawner::Explode(const Vector3& center, float radius, int damage, const Enemy& directHit) {
	// 爆発の範囲を球とし、敵の判定球と重なっていれば巻き込む(弾と敵の判定と同じ球同士の判定)。
	// 直撃した敵を別扱いにするのは、ImGuiで爆発の半径を弾の半径より小さくしても、
	// 当たったのにダメージが入らない状態にしないため
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (!enemy->IsInCombat()) {
			continue; // 同フレームに撃破済みか、通過済み
		}
		if (enemy.get() == &directHit || IsCollision(center, radius, HitCenter(*enemy), ParamsOf(*enemy).radius)) {
			enemy->ApplyDamage(damage, ParamsOf(*enemy).maxHp);
		}
	}

	// 爆発の見た目と音は、弾が消えるときの演出としてBulletManagerが出す(Killを呼んだ時点で出ている)。
	// ここはダメージ(ゲームのルール)だけを担当し、回数はシーンの手応え演出(揺れ・ヒットストップ)に使う
	++explosionCount_;
}

void EnemySpawner::RecordOutcome(const Enemy& enemy, SpawnPoint::Outcome outcome) {
	// 添字はBuildFromStageで敵ごと作り直すので常に有効なはずだが、範囲外でも落とさない(記録が残らないだけ)
	if (enemy.GetSpawnIndex() < spawnPoints_.size()) {
		spawnPoints_[enemy.GetSpawnIndex()].outcome = outcome;
	}
}

void EnemySpawner::UpdatePlayerCollision(const Vector3& playerPosition, float playerRadius) {
	// 1体でも触れていれば同じ結果(ダメージは無敵時間で1回にまとめられる)なので、
	// 見つかった時点で打ち切る
	playerHit_ = false;
	if (!ramEnabled_) {
		return; // 体当たりOFF: その場に留まる敵へ自機が突っ込んでもダメージにしない
	}
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		// 通過済みの敵は離脱中で、自機の後ろ(画面外)に居る。触れても避けようがないので当たらない
		if (enemy->IsPassed()) {
			continue;
		}
		const Vector3 hitCenter = HitCenter(*enemy);
		if (IsCollision(playerPosition, playerRadius, hitCenter, ParamsOf(*enemy).radius)) {
			playerHit_ = true;
			playerHitPosition_ = hitCenter;
			return;
		}
	}
}

void EnemySpawner::PlayApproachSound() {
	if (!approachSound_) {
		return; // 音声ファイルが無ければ鳴らさない(ゲームは止めない)
	}
	SoundManager* soundManager = SoundManager::GetInstance();
	// 同時に鳴らすのは1つまで。1つ鳴っていれば合図としては足りる
	if (soundManager->IsPlaying(approachSoundHandle_)) {
		return;
	}
	approachSoundHandle_ = soundManager->PlayWave(approachSound_, false, SoundManager::SoundCategory::SE);
	soundManager->SetVolume(approachSoundHandle_, kApproachVolume);
}

void EnemySpawner::UpdateEscape(float deltaTime, const Vector3& playerPosition, const Vector3& playerForward) {
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (!enemy->IsPassed()) {
			// 倒すとクリアになる敵(女王)は通過済みにしない。取り逃しになるとクリアできなくなるため
			if (ParamsOf(*enemy).isGoal) {
				continue;
			}
			// 「自機→敵」と進行方向の内積 = 敵が自機より何m前にいるか(負なら後ろ)。
			// 余裕を超えて後ろへ回ったら通過済みにする
			const Vector3 toEnemy = enemy->GetPosition() - playerPosition;
			const float ahead = Vector3::Dot(toEnemy, playerForward);
			if (ahead >= -passMargin_) {
				continue;
			}
			// 離脱の向きは「自機の進路から離れる向き」=画面の中心から外側。
			// 進行方向の成分を引くと、進路に垂直な横ずれだけが残る。
			// 常に上へ逃がさないのは、自機より下にいる敵が画面の中心を横切って上へ抜けてしまうため
			const Vector3 side = toEnemy - playerForward * ahead;
			enemy->StartEscape(side.LengthSquared() > 1.0e-6f
				? Vector3::Normalized(side)
				: Vector3{ 0.0f, 1.0f, 0.0f }); // 真後ろで横ずれが無いときだけ真上へ
			// 通過済みの敵はもう撃ち落とせない(IsInCombatがfalse)ので、離脱し終えるのを待たずにここで取り逃しと確定する
			// (区間の結果がこの瞬間に出せる)
			RecordOutcome(*enemy, SpawnPoint::Outcome::Missed);
		}

		// 追っていた勢いはブレーキで落とし、外側へ抜ける速さを足す
		enemy->UpdateEscape(deltaTime, escapeBrake_, escapeSpeed_);
	}

	// 離脱し終えた敵を取り除く(実体は撃破と同じく墓場へ回す)。
	// 撃破ではないので、撃破演出・撃破音・撃破数は出さない(素通りさせても点が入らないように)
	for (size_t i = enemies_.size(); i > 0; --i) {
		Enemy& enemy = *enemies_[i - 1];
		if (!enemy.IsPassed() || enemy.GetEscapeTime() < escapeLifetime_) {
			continue;
		}
		graveyard_.push_back(enemy.ReleaseObject());
		enemies_.erase(enemies_.begin() + static_cast<ptrdiff_t>(i - 1));
	}
}

bool EnemySpawner::IsAimedAt(const Vector3& point) const {
	// 点と球の判定。半径0の球として既存のIsCollisionに通す(専用の関数を増やさない)
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (enemy->IsInCombat() && IsCollision(point, 0.0f, HitCenter(*enemy), ParamsOf(*enemy).radius)) {
			return true;
		}
	}
	return false;
}

uint32_t EnemySpawner::FindEnemyInCone(const Vector3& origin, const Vector3& direction, float maxAngle,
	float maxDistance) const {
	uint32_t bestId = 0;
	float bestAngle = maxAngle;
	float bestDistance = maxDistance;
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (!enemy->IsInCombat()) {
			continue; // 通過済みの敵にはロックしない(追尾弾が後ろへ引き返してしまう)
		}
		const Vector3 toCenter = HitCenter(*enemy) - origin;
		const float distance = toCenter.Length();
		if (distance < 1.0e-4f || distance > maxDistance) {
			continue; // カメラと重なっている・遠すぎる
		}
		// 視線と「敵の中心への向き」のなす角(内積=cos)。cosが0以下なら真横より後ろなので対象外
		const float cosAngle = Vector3::Dot(toCenter, direction) / distance;
		if (cosAngle <= 0.0f) {
			continue;
		}
		// 敵の見た目の半径(角度)を引いて「視線から敵の縁まで」の角度にする。
		// 中心で比べると、近くの大きな敵ほど縁は照準に掛かっているのに遠い扱いになってしまう
		const float centerAngle = std::acos((std::min)(cosAngle, 1.0f));
		const float angularRadius = std::asin((std::min)(ParamsOf(*enemy).radius / distance, 1.0f));
		const float edgeAngle = (std::max)(centerAngle - angularRadius, 0.0f);
		if (edgeAngle > bestAngle) {
			continue;
		}
		// 角度が同じ(どちらも照準に掛かっている=0)なら手前の敵を選ぶ
		if (edgeAngle == bestAngle && distance >= bestDistance) {
			continue;
		}
		bestId = enemy->GetId();
		bestAngle = edgeAngle;
		bestDistance = distance;
	}
	return bestId;
}

bool EnemySpawner::TryGetEnemyPosition(uint32_t enemyId, Vector3& outPosition) const {
	const Enemy* enemy = FindEnemyById(enemyId);
	if (!enemy) {
		return false;
	}
	outPosition = HitCenter(*enemy); // 追尾弾とロックの狙い先は判定の中心
	return true;
}

bool EnemySpawner::TryGetEnemySphere(uint32_t enemyId, Vector3& outCenter, float& outRadius) const {
	const Enemy* enemy = FindEnemyById(enemyId);
	if (!enemy) {
		return false;
	}
	outCenter = HitCenter(*enemy);
	outRadius = ParamsOf(*enemy).radius;
	return true;
}

const Enemy* EnemySpawner::FindEnemyById(uint32_t enemyId) const {
	// 番号は1から振っているので、0(なし)を渡すと必ず見つからずnullptrになる。
	// 通過済みの敵も見つからない扱いにする: ロック中の敵が通過したらシーンがロックを外し、
	// 飛んでいる追尾弾も追うのをやめる(後ろへ引き返して画面外へ消えないように)
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (enemy->GetId() == enemyId && enemy->IsInCombat()) {
			return enemy.get();
		}
	}
	return nullptr;
}

void EnemySpawner::Update(float railDistance, float deltaTime, const Vector3& playerPosition, float playerRadius,
	const Vector3& playerForward) {
	// 前フレームのコマンドはPostDrawのフェンス待ちで実行完了済みなので、退避分をここで破棄する
	graveyard_.clear();

	// 巣の全滅(クリア演出)の間は、発生・被弾・移動・攻撃・接触を行わず、落ちていく敵を動かすだけにする。
	// 女王を倒した後に新しい敵が出てきたり、固まった敵に撃たれたりしないようにする
	if (frozen_) {
		playerHit_ = false;
		UpdateFrozen(deltaTime);
		return;
	}

	// 敵へ渡すこのフレームの状況(全種で同じもの)。敵はこれを読むだけで、シーンやこのクラスを知らない
	Enemy::Context context;
	context.deltaTime = deltaTime;
	context.playerPosition = playerPosition;
	context.playerForward = playerForward;
	context.enemyBulletManager = enemyBulletManager_;
	context.shootEnabled = shootEnabled_;
	context.ramEnabled = ramEnabled_;
	context.shootIntervalRandom = shootIntervalRandom_;

	for (size_t spawnIndex = 0; spawnIndex < spawnPoints_.size(); ++spawnIndex) {
		SpawnPoint& spawnPoint = spawnPoints_[spawnIndex];
		// 到達した瞬間に1度だけ発生させる(Resetするまで再発生しない)
		if (spawnPoint.spawned || railDistance < spawnPoint.railDistance) {
			continue;
		}
		spawnPoint.spawned = true;

		// 種別名から派生クラスの実体を作り、以降は基底のポインタで扱う
		std::unique_ptr<Enemy> enemy = CreateEnemy(spawnPoint.enemy);
		const size_t typeIndex = TypeIndexOf(spawnPoint.enemy);
		std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
		object->Initialize(Object3dCommon::GetInstance());
		object->SetModel(ModelPathOf(spawnPoint.enemy));
		// 出現位置はSpawnPointのtransform(エディタのギズモで置いた場所)。
		// 大きさだけは種別ごとの既定を使い、データ側のscaleは向き・位置に集中させる
		object->SetTransform(spawnPoint.transform);
		const float scale = ScaleOf(spawnPoint.enemy);
		object->SetScale({ scale, scale, scale });
		if (camera_) {
			object->SetCamera(camera_);
		}
		enemy->Initialize(nextEnemyId_++, spawnIndex, typeIndex, std::move(object), typeParams_[typeIndex], context);

		enemies_.push_back(std::move(enemy));
	}

	// 被弾判定は移動より先に行う。弾は既にこのフレーム分進んだ後なので、
	// 「弾が到達した位置」と「敵の現在位置」で判定でき、1フレームのズレが出ない。
	// 通常弾とチャージ弾は同じ関数で判定する(違いはダメージと爆発の半径という弾のデータだけ)
	const int explosionCountBefore = explosionCount_;
	UpdateBulletCollision(bulletManager_);
	UpdateBulletCollision(chargeBulletManager_);

	// 撃破音は1フレームに1回まで。巻き込みで何体倒しても、同じ音が重なって割れないようにする。
	// このフレームに爆発があったなら鳴らさない: 爆発音(チャージ弾が消えるときの音)がもう鳴っているため
	bool defeatSoundPlayed = explosionCount_ != explosionCountBefore;

	// 撃破された敵を取り除く(実体は墓場へ回して次フレーム冒頭で破棄する)
	for (size_t i = enemies_.size(); i > 0; --i) {
		Enemy& enemy = *enemies_[i - 1];
		if (!enemy.IsDefeated()) {
			continue;
		}
		// 撃破演出。敵の実体が消える前にその位置で発生させる。
		// パーティクルはParticleManagerが所有して寿命まで動かすので、
		// ここで敵を破棄しても演出は残る(発生源の生存に依存しない)
		// 出す位置は判定の中心(体の前寄り)。長い体の敵で、重心から出して体の中に隠れないようにする
		ParticleManager::GetInstance()->Emit(
			kDefeatEffect, HitCenter(enemy), static_cast<uint32_t>(defeatParticleCount_));

		if (defeatSound_ && !defeatSoundPlayed) {
			const SoundManager::SoundHandle handle = SoundManager::GetInstance()->PlayWave(
				defeatSound_, false, SoundManager::SoundCategory::SE);
			SoundManager::GetInstance()->SetVolume(handle, kDefeatVolume);
			defeatSoundPlayed = true;
		}

		// 女王なら、倒れた瞬間の姿を控える(クリア演出が同じ場所・姿勢・大きさに亡骸を置き直す)
		if (ParamsOf(enemy).isGoal) {
			goalDefeatPose_.enemy = spawnPoints_[enemy.GetSpawnIndex()].enemy;
			goalDefeatPose_.transform = enemy.GetPose();
			goalDefeatPose_.center = HitCenter(enemy);
			goalDefeatPose_.radius = ParamsOf(enemy).radius;
			hasGoalDefeatPose_ = true;
		}

		RecordOutcome(enemy, SpawnPoint::Outcome::Defeated);
		graveyard_.push_back(enemy.ReleaseObject());
		enemies_.erase(enemies_.begin() + static_cast<ptrdiff_t>(i - 1));
		++defeatedCount_;
	}

	// 自機より後ろへ回った敵を通過済みにして離脱させる。
	// 追尾より先に行い、通過済みになったフレームから追わせない
	UpdateEscape(deltaTime, playerPosition, playerForward);

	// 種別ごとの振る舞い(どう動き、どう攻めるか)。基底のポインタ経由で呼ぶので、
	// どの派生クラスの処理が走るかは実体の型で決まり、ここには種別の分岐が無い(種別を足してもこのループは変わらない)。
	// 射撃型は間合いで止まって撃ち、体当たり型は判定半径の和より内側まで詰めて体当たりになる。
	// 移動の後に撃つので、弾は「今いる位置」から自機へ向けて出る
	bool approached = false;
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		// 通過済みの敵はUpdateEscapeで動かし済みなので、攻めさせない(見た目の更新だけ行う)
		if (!enemy->IsPassed()) {
			const Enemy::BehaviorResult result = enemy->UpdateBehavior(ParamsOf(*enemy), context);
			approached = approached || result.approached;
			shotCount_ += result.shotCount;
		}
		enemy->UpdateObject(deltaTime);
	}

	// 接近音。間合いに入った敵がいれば、同時に1つまでに絞って鳴らす
	if (approached) {
		PlayApproachSound();
	}

	// 自機との接触判定(A-5)。移動後の位置で行うので、寄ってきた敵がその場で当たる
	UpdatePlayerCollision(playerPosition, playerRadius);
}

void EnemySpawner::Reset() {
	// ImGuiのボタン(Respawn/Reset)から呼ばれるため、こちらも即deleteしない
	RetireEnemies();
	for (SpawnPoint& spawnPoint : spawnPoints_) {
		spawnPoint.spawned = false;
		spawnPoint.outcome = SpawnPoint::Outcome::Pending; // 区間の集計もこれで開始時に戻る
	}
	defeatedCount_ = 0;
	explosionCount_ = 0;
	// 全滅(クリア演出)の状態も開始時に戻す
	frozen_ = false;
	hasGoalDefeatPose_ = false;
}

void EnemySpawner::Draw() {
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		enemy->Draw();
	}
}

void EnemySpawner::DrawHitSpheres() const {
	// 橙にして、自機・地形の判定(緑)と見分ける
	const Vector4 color = { 1.0f, 0.6f, 0.1f, 1.0f };
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (enemy->IsInCombat()) {
			DebugRenderer::GetInstance()->AddSphere(HitCenter(*enemy), ParamsOf(*enemy).radius, color);
		}
	}
}

void EnemySpawner::SetCamera(Camera* camera) {
	camera_ = camera;
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		enemy->SetCamera(camera);
	}
}

void EnemySpawner::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Enemy");
	// 離脱中(通過済み)の数はAliveの内数。撃ち漏らしが離脱しているかの確認用
	const int escapingCount = static_cast<int>(
		std::count_if(enemies_.begin(), enemies_.end(),
			[](const std::unique_ptr<Enemy>& enemy) { return enemy->IsPassed(); }));
	ImGui::Text("Alive: %zu (Escaping: %d)  Defeated: %d  Explosions: %d",
		enemies_.size(), escapingCount, defeatedCount_, explosionCount_);
	// 体当たりが成立しているかの確認用(Keep Distanceを判定半径の和より大きくすると触れなくなる)
	if (playerHit_) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Touching Player: YES");
	} else {
		ImGui::Text("Touching Player: no");
	}

	// 種別ごとのタブ。体当たり型と射撃型を別々に詰められるようにする
	// (弾の速さ・寿命・見た目は"Enemy Bullet"ウィンドウ側で調整する)
	ImGui::SeparatorText("Types");
	if (ImGui::BeginTabBar("EnemyTypes")) {
		for (EnemyTypeParams& params : typeParams_) {
			if (!ImGui::BeginTabItem(params.name.c_str())) {
				continue;
			}
			ImGui::PushID(params.name.c_str()); // 同じラベルを種別ごとに使うため
			// HPと判定半径の変更は次に発生する敵から効く(HPは発生時に確定するため)
			ImGui::DragInt("Max HP", &params.maxHp, 0.1f, 1, 20);
			ImGui::DragFloat("Hit Radius", &params.radius, 0.1f, 0.1f, 20.0f, "%.1f m");
			// 判定の中心を頭の方へずらす量。毎フレーム引き直すので、発生済みの敵にも即座に効く
			ImGui::DragFloat("Hit Offset", &params.hitOffset, 0.1f, -50.0f, 50.0f, "%.1f m");
			// 移動と攻撃は毎フレーム引き直すので、発生済みの敵にも即座に効く
			ImGui::DragFloat("Move Speed", &params.moveSpeed, 0.1f, 0.0f, 30.0f, "%.1f m/s");
			ImGui::DragFloat("Keep Distance", &params.keepDistance, 0.1f, 0.0f, 80.0f, "%.1f m");
			// 撃つか・体当たりするかは数値ではなく派生クラス(CreateEnemyの対応)で決まるので、ここでは切り替えない。
			// 種別に関係のない欄も並ぶが、読むのはその振る舞いのクラスだけ(射撃型が接近音の範囲を読むことはない)
			ImGui::SeparatorText("Shooter (frog / queen)");
			ImGui::DragFloat("Fire Interval", &params.shootInterval, 0.05f, 0.1f, 10.0f, "%.2f s");
			ImGui::DragFloat("Fire Range", &params.shootRange, 1.0f, 1.0f, 300.0f, "%.0f m");
			ImGui::SeparatorText("Rammer (sword)");
			// 0で鳴らさない。変更は鳴らしていない個体に即座に効く
			ImGui::DragFloat("Approach Sound Range", &params.approachSoundRange, 0.5f, 0.0f, 200.0f, "%.1f m");
			ImGui::SeparatorText("Rule");
			// 倒すとクリアになるか。ONの種別がステージにいると、終点でレールが止まり、倒すまで待つ
			ImGui::Checkbox("Goal (defeat to clear)", &params.isGoal);
			ImGui::PopID();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::SeparatorText("Common");
	ImGui::Checkbox("Shoot Enabled (all types)", &shootEnabled_);
	ImGui::SameLine();
	ImGui::Text("Shots: %d", shotCount_);
	ImGui::Checkbox("Ram Enabled (all types)", &ramEnabled_);
	ImGui::DragFloat("Interval Random", &shootIntervalRandom_, 0.01f, 0.0f, 0.9f, "%.2f");
	// 撃破演出の粒数。configそのものは"Particle"ウィンドウ側でライブ編集できる
	ImGui::DragInt("Defeat Particles", &defeatParticleCount_, 0.5f, 0, 200);
	// 爆発の半径・ダメージ・見た目(粒数)は弾の性能なので"Charge Bullet"ウィンドウ側にある

	// 自機より後ろへ回った敵の離脱。毎フレーム引き直すので、離脱中の敵にも即座に効く
	ImGui::SeparatorText("Passed");
	ImGui::DragFloat("Pass Margin", &passMargin_, 0.1f, 0.0f, 30.0f, "%.1f m");
	ImGui::DragFloat("Escape Speed", &escapeSpeed_, 0.1f, 0.0f, 50.0f, "%.1f m/s");
	ImGui::DragFloat("Escape Brake", &escapeBrake_, 0.1f, 0.0f, 50.0f, "%.1f m/s^2");
	ImGui::DragFloat("Escape Lifetime", &escapeLifetime_, 0.05f, 0.1f, 10.0f, "%.2f s");

	// 女王を倒した後の巣の全滅(クリア演出)。煙の見た目は"Particle"ウィンドウ側でライブ編集できる
	ImGui::SeparatorText("Exterminate (clear)");
	ImGui::Text("Frozen: %s", frozen_ ? "yes" : "no");
	ImGui::DragFloat("Fall Gravity", &exterminateGravity_, 0.5f, 0.0f, 200.0f, "%.1f m/s^2");
	ImGui::DragFloat("Life Min", &exterminateLifeMin_, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::DragFloat("Life Max", &exterminateLifeMax_, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::DragInt("Smoke Particles", &exterminateSmokeCount_, 0.5f, 0, 100);
	ImGui::End();
#endif
}

EnemySpawner::EnemySpawner() {
	// 種別ごとの既定値(数値)。どう動き、どう攻めるか(振る舞い)はCreateEnemyが選ぶ派生クラスが決める
	EnemyTypeParams frog;
	frog.name = "frog";
	frog.maxHp = 3;
	frog.radius = 3.0f;        // 幅6mの箱に合わせる(ScaleOf)
	frog.moveSpeed = 2.0f;     // ゆっくり間合いを詰める(撃つ時間を作る)
	frog.keepDistance = 22.0f; // 手前で止まり、そこから撃ち続ける
	frog.shootInterval = 2.2f;
	frog.shootRange = 90.0f;

	EnemyTypeParams sword;
	sword.name = "sword";
	sword.maxHp = 2;           // 突っ込んでくる分HPは低く。撃ち落とすか避けるかの二択にする
	sword.radius = 2.0f;       // 幅4mの箱に合わせる(ScaleOf)
	sword.moveSpeed = 9.0f;    // 一気に間合いを詰める
	sword.keepDistance = 1.5f; // 判定半径の和(自機0.5+敵2.0=2.5m)より内側=必ず体当たりになる
	// 9m/sで詰めてくるので、30m手前で鳴らして避ける猶予を作る
	// (敵だけなら接触の約3秒前。自機もレール上を前進するので実際はそれより短い)
	sword.approachSoundRange = 30.0f;

	// 女王(仮)。終点の先に置き、倒すとクリアになる。動きの作り込みは「敵やギミックの挙動」の回で行うので、
	// 今は射撃型(ShooterEnemy)の数値だけで「動かずに撃ってくる、硬くて大きい的」にしておく
	EnemyTypeParams queen;
	queen.name = "queen";
	queen.maxHp = 24;           // 通常弾24発 / チャージ弾(3ダメージ)8発。終点で少し粘る硬さ
	queen.radius = 6.0f;        // 胴の最大幅(約12m。ScaleOf)の半分。真正面から見た輪郭に合わせる
	// 判定の球を頭の方へ6mずらす。全長約39mのうち、頭の先は重心から約25m前にあり、
	// 正面から見えている腹の前面も重心より15m以上手前にある。重心のままだと弾が体の奥で当たって
	// 着弾の火花が隠れるので、球の前端(重心から12m前)が見えている前面の近くに来るよう寄せる
	queen.hitOffset = 6.0f;
	queen.moveSpeed = 0.0f;     // その場から動かない(終点の先で待ち構える)
	queen.keepDistance = 0.0f;
	queen.shootInterval = 1.6f; // 射撃型より短い間隔で撃ち、止まっている間も避ける必要を作る
	queen.shootRange = 130.0f;  // 遠くから見えている間に撃ち始める(終点の約60m先に置く前提)
	queen.isGoal = true;

	typeParams_ = { frog, sword, queen }; // 並びはEnemyTypes()と同じ
}

EnemySpawner::~EnemySpawner() = default;
