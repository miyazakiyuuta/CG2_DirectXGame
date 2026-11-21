#include "direction/GameOverSequence.h"

#include "3d/Camera.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "effect/EffectManager.h"
#include "effect/Monochrome.h"
#include "effect/ParticleEmitter.h"
#include "effect/ParticleManager.h"
#include "effect/RadialBlur.h"
#include "effect/Vignette.h"
#include "math/Easing.h"
#include "math/Matrix4x4.h"
#include "player/Player.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// パーティクルのグループ名。登録(コンストラクタ・RegisterEffect)・発生・後片付け(ClearParticles)で同じ名前を使う
	const std::string kSmokeEffect = "gameOverSmoke";
	const std::string kSparkEffect = "gameOverSpark";
	const std::string kExplosionEffect = "gameOverExplosion";
	// 粒の画像はブレンドの仕方で使い分ける。
	// circle.png は黒背景に白い丸を明るさだけで描いた加算用の画像(αは全面255)。加算なら黒は足しても0で見えないが、
	// αブレンドでは黒い部分も不透明に描かれ、四角い板が見えてしまう。
	// smoke.png はαに丸の形を持たせた画像(RGBは白)。色は config の startColor/endColor で決め、画像は形だけを持つ
	const std::string kSmokeTexture = "resources/smoke.png";       // αブレンド用(煙)
	const std::string kAdditiveTexture = "resources/circle.png";   // 加算用(火花・爆発)

	// 爆発音(チャージ弾・敵の撃破と同じ素材)
	const std::string kExplosionSound = "resources/sounds/explosion.mp3";
	constexpr float kExplosionVolume = 0.9f;

	// 被弾箇所を置く楕円体の半径[m](機体のローカル: x=右 / y=上 / z=前)。
	// 自機モデルの実寸(約1.1 x 0.26 x 0.74m)の半分くらいにして、当たった方向の機体の表面あたりに置く
	const Vector3 kHitEllipsoid = { 0.5f, 0.12f, 0.35f };
	// どこから当たったか分からないとき(ImGuiのDamageボタン・Force GameOver)の被弾箇所の向き。右翼の付け根のやや後ろ
	const Vector3 kFallbackHitDirection = { 1.0f, 0.2f, -0.3f };

	// Vignetteの形。通常の被弾演出(GamePlayScene::UpdateDamageEffect)と同じ形にして、
	// 致命的な被弾の瞬間に赤の出方が切り替わって見えないようにする
	const Vector3 kVignetteRed = { 1.0f, 0.0f, 0.0f };
	constexpr float kVignetteRadius = 0.22f;
	constexpr float kVignetteSoftness = 0.55f;
	constexpr float kVignettePower = 1.4f;
	// 暗転し切ったときの半径。Vignetteは中心からの距離が radius+softness より外側を色に寄せるので、
	// radius+softness が0未満になると画面の中心まで色(黒)になる。半径を縮めるだけで「真っ黒」まで閉じられる
	constexpr float kClosedRadius = -(kVignetteSoftness + 0.05f);

	// RadialBlurの形(Startでまとめて設定する。RadialBlurは全シーン共有なので、使う側が毎回すべて決める)
	constexpr float kBlurWidth = 0.015f;
	constexpr int kBlurSamples = 12;
	constexpr float kBlurFocusRadius = 0.08f; // 機体のまわりだけ鮮明に残す
	constexpr float kBlurFocusSoftness = 0.35f;

	// ImGui表示用の段階名(Phaseの並びと揃える)
	const char* const kPhaseNames[] = { "None", "Impact", "Spin", "Drift", "Explode", "FadeOut", "Finished" };

	// 効果音を1回鳴らす(音声ファイルが読めていなければ何もしない=ゲームは止めない)
	void PlaySe(const std::shared_ptr<const SoundData>& sound, float volume) {
		if (!sound) {
			return;
		}
		const SoundManager::SoundHandle handle =
			SoundManager::GetInstance()->PlayWave(sound, false, SoundManager::SoundCategory::SE);
		SoundManager::GetInstance()->SetVolume(handle, volume);
	}

	// 名前でポストエフェクトを引いて具象型にする。登録が外れていてもnullptrを返すだけで落とさない
	// (演出は無くてもゲームは成立する)
	template <class T>
	T* FindPostEffect(EffectManager* effectManager, const char* name) {
		if (!effectManager) {
			return nullptr;
		}
		return static_cast<T*>(effectManager->FindEffect(name));
	}
}

GameOverSequence::GameOverSequence() = default;

GameOverSequence::~GameOverSequence() = default;

void GameOverSequence::Initialize(EffectManager* effectManager) {
	effectManager_ = effectManager;

	// 墜落する機体。自機と同じモデル・大きさにして、入れ替わった瞬間が見分けられないようにする
	ModelManager::GetInstance()->LoadModel(Player::ModelPath());
	wreck_ = std::make_unique<Object3d>();
	wreck_->Initialize(Object3dCommon::GetInstance());
	wreck_->SetModel(Player::ModelPath());
	wreck_->SetScale({ Player::ModelScale(), Player::ModelScale(), Player::ModelScale() });

	// 煙。少しずつ上へ昇りながら膨らんで消える。背景が黒なので、暗すぎる灰色だと見えなくなる。
	// 速さを小さくしてあるので、機体が動くと出所に取り残されて軌跡になる
	ParticleConfig smokeConfig;
	smokeConfig.minScale = { 0.25f, 0.25f, 0.25f };
	smokeConfig.maxScale = { 0.45f, 0.45f, 0.45f };
	smokeConfig.minVelocity = { -0.4f, 0.3f, -0.4f };
	smokeConfig.maxVelocity = { 0.4f, 1.2f, 0.4f };
	smokeConfig.lifeTimeMin = 1.0f;
	smokeConfig.lifeTimeMax = 1.8f;
	smokeConfig.startColor = { 0.55f, 0.53f, 0.5f, 0.7f };
	smokeConfig.endColor = { 0.2f, 0.2f, 0.2f, 0.0f };
	smokeConfig.endScaleRatio = 3.5f; // 広がりながら薄くなる
	// 煙は光らないので加算ではなくαブレンド(加算だと重なるほど白く光ってしまう)
	smokeEmitter_ = std::make_unique<ParticleEmitter>(kSmokeEffect, kSmokeTexture, smokeConfig, BlendMode::Alpha);

	// 火花。小さく速く飛び散り、重力で落ちながら黄色→橙へ冷えて消える
	ParticleConfig sparkConfig;
	sparkConfig.minScale = { 0.06f, 0.06f, 0.06f };
	sparkConfig.maxScale = { 0.14f, 0.14f, 0.14f };
	sparkConfig.minVelocity = { -4.0f, -1.0f, -4.0f };
	sparkConfig.maxVelocity = { 4.0f, 5.0f, 4.0f };
	sparkConfig.acceleration = { 0.0f, -9.8f, 0.0f };
	sparkConfig.lifeTimeMin = 0.25f;
	sparkConfig.lifeTimeMax = 0.6f;
	sparkConfig.startColor = { 1.0f, 0.95f, 0.6f, 1.0f };
	sparkConfig.endColor = { 1.0f, 0.35f, 0.05f, 0.0f };
	sparkConfig.endScaleRatio = 0.5f;
	sparkEmitter_ = std::make_unique<ParticleEmitter>(kSparkEffect, kAdditiveTexture, sparkConfig, BlendMode::Add);

	// エミッタは作った時点で出し始める(既定がactive)ので、演出が始まるまで止めておく。
	// 1回に1粒ずつ出し、量は発生間隔(=1÷毎秒の粒数)で決める
	smokeEmitter_->SetActive(false);
	smokeEmitter_->SetCount(1);
	sparkEmitter_->SetActive(false);
	sparkEmitter_->SetCount(1);

	// 爆発の炎。敵の撃破の爆発(0.4〜0.9)より大きくする: 機体は20m以上先で爆発するので、
	// 同じ大きさだと小さすぎて「自機が爆発した」重みが出ない
	explosionConfig_.minScale = { 0.8f, 0.8f, 0.8f };
	explosionConfig_.maxScale = { 2.0f, 2.0f, 2.0f };
	explosionConfig_.minVelocity = { -8.0f, -8.0f, -8.0f };
	explosionConfig_.maxVelocity = { 8.0f, 8.0f, 8.0f };
	explosionConfig_.acceleration = { 0.0f, -1.0f, 0.0f };
	explosionConfig_.lifeTimeMin = 0.4f;
	explosionConfig_.lifeTimeMax = 1.0f;
	explosionConfig_.startColor = { 1.0f, 0.9f, 0.55f, 1.0f };
	explosionConfig_.endColor = { 0.9f, 0.2f, 0.0f, 0.0f };
	explosionConfig_.endScaleRatio = 0.3f;
	ParticleManager::GetInstance()->RegisterEffect(kExplosionEffect, kAdditiveTexture, explosionConfig_, BlendMode::Add);

	explosionSound_ = SoundManager::GetInstance()->LoadFile(kExplosionSound);
}

void GameOverSequence::Start(const Vector3& position, const Vector3& rotate, const Vector3* hitSource) {
	position_ = position;
	startRotate_ = rotate;
	// 機首の向き(行ベクトル規約では、ローカルの+Zに回転を掛けたものがワールドでの機首の向き)
	forward_ = Matrix4x4::Rotate(rotate).Transform({ 0.0f, 0.0f, 1.0f });
	velocity_ = forward_ * params_.startSpeed;
	roll_ = 0.0f;
	tumble_ = 0.0f;
	cameraFollow_ = 0.0f;
	elapsed_ = 0.0f;
	localHitPoint_ = ComputeLocalHitPoint(position, rotate, hitSource);

	EnterPhase(Phase::Impact);
	timeScale_ = ComputeTimeScale();

	// このフレームの描画に間に合わせる(自機を描かなくなるフレームから、同じ姿勢の機体が見えているように)
	ApplyWreckPose();
	wreck_->Update(0.0f);

	// 被弾の瞬間の火花。ヒットストップ中も粒は等速で流れる(止まった画面の中で火花だけが散る)
	sparkEmitter_->EmitAt(hitPoint_, static_cast<uint32_t>((std::max)(params_.impactSparkCount, 0)));

	// RadialBlurの形を決め、被弾の瞬間だけ強く掛ける(Triggerは強さを一瞬上げて自動で戻す)
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		radialBlur->SetBlurWidth(kBlurWidth);
		radialBlur->SetNumSamples(kBlurSamples);
		radialBlur->SetFocusRadius(kBlurFocusRadius);
		radialBlur->SetFocusSoftness(kBlurFocusSoftness);
		radialBlur->SetAspect(1.0f);
		radialBlur->SetIntensity(0.0f);
		radialBlur->Trigger(params_.impactBlur, params_.impactBlurDuration);
	}
}

void GameOverSequence::Stop() {
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	elapsed_ = 0.0f;
	timeScale_ = 1.0f;
	cameraFollow_ = 0.0f;

	// 時間倍率はParticleManagerの全グループに掛かる。戻さないと撃破の爆発までスローのままになる
	ParticleManager::GetInstance()->SetTimeScale(1.0f);
	smokeEmitter_->SetActive(false);
	sparkEmitter_->SetActive(false);
	// 粒はワールド座標で残り続けるので、Edit・リトライ・結果画面へ持ち越さないよう消す
	ParticleManager::GetInstance()->ClearParticles(kSmokeEffect);
	ParticleManager::GetInstance()->ClearParticles(kSparkEffect);
	ParticleManager::GetInstance()->ClearParticles(kExplosionEffect);

	// ポストエフェクトは全シーン共有なので、掛けたまま残さない。
	// Vignetteは通常の被弾演出と共有だが、そちらは毎フレームON/OFFを決め直すので、ここで切っても困らない
	if (Vignette* vignette = FindPostEffect<Vignette>(effectManager_, "Vignette")) {
		vignette->SetEnabled(false);
	}
	if (Monochrome* monochrome = FindPostEffect<Monochrome>(effectManager_, "Monochrome")) {
		monochrome->SetEnabled(false);
	}
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		radialBlur->SetEnabled(false);
		radialBlur->SetIntensity(0.0f); // 常時掛けの強さはシーンをまたいで残るので0へ戻す
	}
}

void GameOverSequence::Skip() {
	// 受け付けるのは制御不能・遠ざかる の途中だけ。被弾の瞬間(0.25秒)は短いので飛ばさせず、
	// 爆発より後はもう暗転へ向かっている。爆発へ進めるのは、機体がその場で爆発すれば話がつながるため
	// (暗転へ直接飛ぶと、機体が爆発せずに消えてしまう)
	if (phase_ == Phase::Spin || phase_ == Phase::Drift) {
		EnterPhase(Phase::Explode);
	}
}

void GameOverSequence::Update(float deltaTime, const Camera& camera) {
	// 真っ黒になった後は何も触らない。シーンが結果画面への遷移を頼むと、ChangeSceneが全ポストエフェクトを切る
	// (同じフレームから遷移の黒い帯が画面を覆う)。ここで点け直すと次のシーンへ点いたまま渡ってしまう
	// (TitleSceneの出発の演出と同じ注意)
	if (phase_ == Phase::None || phase_ == Phase::Finished) {
		return;
	}

	// 段階を進める。長さはすべて実時間なので、スローモーションの間も演出の長さは変わらない
	phaseTimer_ += deltaTime;
	elapsed_ += deltaTime;
	switch (phase_) {
	case Phase::Impact:
		if (phaseTimer_ >= params_.impactDuration) {
			EnterPhase(Phase::Spin);
		}
		break;
	case Phase::Spin:
		if (phaseTimer_ >= params_.spinDuration) {
			EnterPhase(Phase::Drift);
		}
		break;
	case Phase::Drift:
		if (phaseTimer_ >= params_.driftDuration) {
			EnterPhase(Phase::Explode);
		}
		break;
	case Phase::Explode:
		if (phaseTimer_ >= params_.explodeDuration) {
			EnterPhase(Phase::FadeOut);
		}
		break;
	case Phase::FadeOut:
		if (phaseTimer_ >= params_.fadeDuration) {
			EnterPhase(Phase::Finished);
		}
		break;
	default:
		break;
	}

	// ゲーム時間の倍率。シーンは次のフレームから、敵・弾のdeltaTimeにもこの倍率を掛ける
	// (1フレーム(約16ms)の遅れは体感できない)
	timeScale_ = ComputeTimeScale();
	// 粒にも同じ倍率を掛け、スロー中は煙と火花も遅くする。ただしヒットストップ(倍率0)の間は等速のまま流す:
	// 通常の被弾と同じく「止まった画面の中で火花だけが散る」見せ方にするため
	ParticleManager::GetInstance()->SetTimeScale(phase_ == Phase::Impact ? 1.0f : timeScale_);

	if (IsWreckVisible()) {
		UpdateWreck(deltaTime * timeScale_);
	}

	// カメラが機体を目で追う割合。遠ざかる間に少しずつ上げ、爆発した後はその向きのまま見届ける
	if (phase_ == Phase::Drift) {
		const float t = params_.driftDuration > 0.0f ? phaseTimer_ / params_.driftDuration : 1.0f;
		cameraFollow_ = params_.cameraFollow * Easing::InOutSine(t);
	}

	UpdateEmitters();

	// 真っ黒になったフレームも、真っ黒の状態をここで1度だけ書き込む(次のフレームからは上の早期returnで触らない)
	ApplyPostEffects(camera);
}

void GameOverSequence::EnterPhase(Phase phase) {
	phase_ = phase;
	phaseTimer_ = 0.0f;

	if (phase == Phase::Explode) {
		Explode();
	}
}

float GameOverSequence::ComputeTimeScale() const {
	switch (phase_) {
	case Phase::Impact:
		return 0.0f; // ヒットストップ
	case Phase::Spin:
		return params_.slowTimeScale;
	case Phase::Drift: {
		// スローから等速へ、なめらかに戻す
		const float t = params_.recoverDuration > 0.0f ? phaseTimer_ / params_.recoverDuration : 1.0f;
		return std::lerp(params_.slowTimeScale, 1.0f, Easing::InOutSine(t));
	}
	default:
		return 1.0f;
	}
}

float GameOverSequence::GetFadeProgress() const {
	if (phase_ == Phase::Finished) {
		return 1.0f;
	}
	if (phase_ != Phase::FadeOut || params_.fadeDuration <= 0.0f) {
		return 0.0f;
	}
	return std::clamp(phaseTimer_ / params_.fadeDuration, 0.0f, 1.0f);
}

bool GameOverSequence::IsWreckVisible() const {
	return phase_ == Phase::Impact || phase_ == Phase::Spin || phase_ == Phase::Drift;
}

Vector3 GameOverSequence::ComputeLocalHitPoint(const Vector3& position, const Vector3& rotate, const Vector3* hitSource) const {
	Vector3 direction = kFallbackHitDirection;
	if (hitSource) {
		const Vector3 toSource = *hitSource - position;
		if (toSource.LengthSquared() > 1.0e-6f) {
			// ワールドの向きを機体のローカルへ測り直す。機体の右・上・前の各軸との内積が、その軸の方向の成分になる
			// (行ベクトル規約では、ローカルの各軸に回転を掛けたものがワールドでの各軸の向き)
			const Matrix4x4 rotation = Matrix4x4::Rotate(rotate);
			const Vector3 right = rotation.Transform({ 1.0f, 0.0f, 0.0f });
			const Vector3 up = rotation.Transform({ 0.0f, 1.0f, 0.0f });
			const Vector3 forward = rotation.Transform({ 0.0f, 0.0f, 1.0f });
			direction = {
				Vector3::Dot(toSource, right),
				Vector3::Dot(toSource, up),
				Vector3::Dot(toSource, forward) };
		}
	}
	direction.Normalize();
	// 長さ1の向きを機体の大きさの楕円体へ引き伸ばし、当たった方向の機体の表面あたりの点にする
	return { direction.x * kHitEllipsoid.x, direction.y * kHitEllipsoid.y, direction.z * kHitEllipsoid.z };
}

void GameOverSequence::UpdateWreck(float gameDeltaTime) {
	// 前方へ流す力は「遠ざかる」の間だけ。落ちる力は制御を失った後ずっと掛かる。
	// すべてゲーム時間で積むので、スロー中は動きも回転も遅くなる(ヒットストップ中は止まる)
	if (phase_ == Phase::Drift) {
		velocity_ += forward_ * (params_.driftAcceleration * gameDeltaTime);
	}
	velocity_.y -= params_.fallAcceleration * gameDeltaTime;
	position_ += velocity_ * gameDeltaTime;
	roll_ += params_.rollSpeed * gameDeltaTime;
	tumble_ += params_.tumbleSpeed * gameDeltaTime;

	ApplyWreckPose();
	wreck_->Update(gameDeltaTime);
}

void GameOverSequence::ApplyWreckPose() {
	// 機体のローカルの回転(機首の軸まわりのロール → 機首を下げる)を先に掛け、その後で被弾した瞬間の向きへ回す。
	// 行ベクトル規約(v × M)では左の行列から順に効くので、「ローカル → ワールド」の順に左から並べる。
	// Object3dのオイラー角のrotate.zに足すだけだと、Zは最後に掛かる(=ワールドのZ軸まわり)ので、
	// レールが曲がった所(yawが0でない所)では機首の軸からずれて斜めに回ってしまう
	const Matrix4x4 rotation =
		Matrix4x4::RotateZ(roll_) * Matrix4x4::RotateX(tumble_) * Matrix4x4::Rotate(startRotate_);
	// Object3dはオイラー角しか受け取らないので、合成した回転行列を角度へ戻して渡す
	// 最後に、モデルの機首を+Zへ向け直す補正を「いちばん最初」(左端)に足す。
	// ロール・機首下げ・被弾時の向きは、機首を+Zとしたまま今までどおり計算できる
	wreck_->SetRotate(Matrix4x4::ToEuler(Player::ModelCorrection() * rotation));
	wreck_->SetTranslate(position_);

	// 被弾箇所にも同じ回転を掛ける(機体に貼り付いた点として一緒に回る)
	hitPoint_ = position_ + rotation.Transform(localHitPoint_);
}

void GameOverSequence::UpdateEmitters() {
	// 煙と火花は、制御を失ってから爆発するまで被弾箇所から出し続ける。
	// 発生のタイミングはParticleManagerが時間倍率つきで数えるので、スロー中は出る量も遅くなる
	const bool burning = phase_ == Phase::Spin || phase_ == Phase::Drift;
	smokeEmitter_->SetActive(burning);
	sparkEmitter_->SetActive(burning);
	smokeEmitter_->SetPosition(hitPoint_);
	sparkEmitter_->SetPosition(hitPoint_);
	// 毎秒の粒数 → 発生間隔。ImGuiで変えた値がすぐ効くよう毎フレーム渡す(0なら出さない)
	smokeEmitter_->SetEmitInterval(params_.smokeRate > 0.0f ? 1.0f / params_.smokeRate : 0.0f);
	sparkEmitter_->SetEmitInterval(params_.sparkRate > 0.0f ? 1.0f / params_.sparkRate : 0.0f);
}

void GameOverSequence::Explode() {
	ParticleManager* particleManager = ParticleManager::GetInstance();

	// 炎
	particleManager->Emit(kExplosionEffect, position_, explosionConfig_,
		static_cast<uint32_t>((std::max)(params_.explosionCount, 0)));

	// 火花。被弾箇所から出ていたものより速く飛ばして、爆発の勢いを出す
	ParticleConfig sparkBurst = sparkEmitter_->GetConfig();
	sparkBurst.minVelocity *= 2.5f;
	sparkBurst.maxVelocity *= 2.5f;
	particleManager->Emit(kSparkEffect, position_, sparkBurst,
		static_cast<uint32_t>((std::max)(params_.explosionSparkCount, 0)));

	// 煙。炎より長く残して、暗転の間の余韻にする(上へ昇るだけでなく全方向へ広げ、爆発の跡の雲にする)
	ParticleConfig smokeBurst = smokeEmitter_->GetConfig();
	smokeBurst.minVelocity = { -2.5f, -1.0f, -2.5f };
	smokeBurst.maxVelocity = { 2.5f, 2.5f, 2.5f };
	smokeBurst.minScale *= 2.0f;
	smokeBurst.maxScale *= 2.0f;
	smokeBurst.lifeTimeMin *= 1.5f;
	smokeBurst.lifeTimeMax *= 1.5f;
	particleManager->Emit(kSmokeEffect, position_, smokeBurst,
		static_cast<uint32_t>((std::max)(params_.explosionSmokeCount, 0)));

	PlaySe(explosionSound_, kExplosionVolume);

	// RadialBlurを一瞬強める(中心は爆発した点。ApplyPostEffectsが毎フレーム合わせる)
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		radialBlur->Trigger(params_.explosionBlur, params_.explosionBlurDuration);
	}
	// 画面の揺れはシーンが持っているので、シーンが段階の変化(Explodeに入った)を見て揺らす
}

void GameOverSequence::ApplyPostEffects(const Camera& camera) {
	// 暗転の進み具合。白黒にし切ることと、黒い縁を閉じることの2つをこの1つの値で動かす
	const float fade = GetFadeProgress();
	const float fadeCurve = Easing::InOutSine(fade);

	// --- Vignette: 被弾の瞬間だけ赤くし、すぐ黒い縁へ移して爆発まで保ち、暗転でその黒い縁を閉じる ---
	// Vignetteは色を1つしか持てないので、「色を赤→黒へ寄せながら濃さを下げる」ことで赤から黒い縁へ移す
	if (Vignette* vignette = FindPostEffect<Vignette>(effectManager_, "Vignette")) {
		// 赤の残り(1=被弾の直後 〜 0=消えた)。普段の被弾演出(UpdateDamageEffect)と同じく、残り時間に比例して抜く。
		// 演出が始まってからの実時間で数えるので、ヒットストップやスローの間も赤は普段の被弾と同じ長さで消える
		const float flash = params_.flashDuration > 0.0f
			? std::clamp(1.0f - elapsed_ / params_.flashDuration, 0.0f, 1.0f)
			: 0.0f;
		// 縁の濃さ: 被弾の直後は赤の濃さ、赤が消えた後は黒い縁の濃さ
		const float edgeIntensity = std::lerp(params_.darkVignette, params_.impactVignette, flash);
		vignette->SetEnabled(true);
		// 色は赤の残りだけで決まり、既定の長さでは暗転が始まる頃にはもう黒になっている。
		// (1 - fadeCurve)を掛けておくのは、ImGuiで赤を暗転まで長くした場合にも、真っ黒で終わるようにするため
		vignette->SetColor(kVignetteRed * (flash * (1.0f - fadeCurve)));
		// 暗転: 黒い縁を濃くしながら半径を縮め、最後は画面の中心まで黒で閉じる
		vignette->SetIntensity(std::lerp(edgeIntensity, 1.0f, fadeCurve));
		vignette->SetRadius(std::lerp(kVignetteRadius, kClosedRadius, fadeCurve));
		vignette->SetSoftness(kVignetteSoftness);
		vignette->SetPower(kVignettePower);
		vignette->SetAspect(1.0f);
	}

	// --- Monochrome: 赤が消えた後から爆発へ向けて色をゆっくり抜き始め、暗転の前半で白黒にし切る ---
	// ポストエフェクトは登録順(RadialBlur → Monochrome → Vignette)に掛かるので、
	// 白黒にした画面の上にVignetteの黒い縁が乗り、その縁が閉じて真っ黒になる
	if (Monochrome* monochrome = FindPostEffect<Monochrome>(effectManager_, "Monochrome")) {
		// 抜けかけの強さ。赤が消えてから、予定どおりなら爆発が始まる時刻までに drainMonochrome へ上げる。
		// 赤と同じく演出が始まってからの実時間で数えるので、スローの間も色の抜け方は遅くならない。
		// 上がり切る時刻は段階の長さから計算するので、ImGuiで段階の長さを変えても爆発の頃に届く
		// (赤が爆発の時刻まで残る長さにした場合は、抜き始める間がないので暗転だけで抜く)
		const float drainStart = params_.flashDuration;
		const float drainEnd = params_.impactDuration + params_.spinDuration + params_.driftDuration;
		const float drainLength = drainEnd - drainStart;
		// カーブはOutCubic(勢いよく動き出し、だんだん減速する)。赤が引くのに続けて、すぐ色も引き始めたように見せる
		// (InOutSineだと動き出しが遅く、赤が消えてから1秒ほど変化が見えない)
		const float drain = drainLength > 0.0f ? Easing::OutCubic((elapsed_ - drainStart) / drainLength) : 0.0f;
		const float drainIntensity = params_.drainMonochrome * drain;
		// 暗転: 前半 desaturateRatio までのカーブで、抜けかけの強さから 1(白黒) へ寄せる。
		// 抜けかけの強さも暗転のカーブも減らないので、Skipで早く爆発へ飛んでも途中で色が戻らない
		const float t = params_.desaturateRatio > 0.0f ? fade / params_.desaturateRatio : 1.0f;
		const float intensity = std::lerp(drainIntensity, 1.0f, Easing::InOutSine(t));
		monochrome->SetEnabled(intensity > 0.0f);
		monochrome->SetColor({ 1.0f, 1.0f, 1.0f }); // 白=グレースケール
		monochrome->SetIntensity(intensity);
	}

	// --- RadialBlur: 機体(爆発後は爆発した点)を中心に放射状にぼかす ---
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		// 常時掛けの強さ。瞬間的な強さ(被弾・爆発)はTriggerが上乗せして自動で戻すので、ここは土台だけ決める
		float baseIntensity = 0.0f;
		if (phase_ == Phase::Spin) {
			baseIntensity = params_.spinBlur;
		} else if (phase_ == Phase::Drift) {
			const float t = params_.driftDuration > 0.0f ? phaseTimer_ / params_.driftDuration : 1.0f;
			baseIntensity = std::lerp(params_.spinBlur, 0.0f, Easing::InOutSine(t));
		}
		radialBlur->SetEnabled(fade <= 0.0f); // 暗転に入ったら外す(爆発の一瞬の強さは爆発の間に抜け切る)
		radialBlur->SetIntensity(baseIntensity);
		radialBlur->SetCenterWorld(position_, camera.GetViewProjectionMatrix());
	}
}

void GameOverSequence::Draw() {
	if (IsWreckVisible()) {
		wreck_->Draw();
	}
}

void GameOverSequence::SetCamera(Camera* camera) {
	wreck_->SetCamera(camera);
}

void GameOverSequence::ComputeCameraRotate(const Vector3& cameraPosition, const Vector3& railForward, Vector3& outRotate) const {
	// 見る向き = レールの向きと「機体への向き」をcameraFollow_の割合で混ぜたもの。
	// 角度(yaw/pitch)ではなく向きのベクトルを混ぜるのは、IntroCameraと同じく、
	// atan2の±πの境目をまたぐと角度の補間が逆回りするため
	Vector3 look = railForward;
	const Vector3 toTarget = position_ - cameraPosition;
	if (cameraFollow_ > 0.0f && toTarget.LengthSquared() > 1.0e-6f) {
		look = railForward * (1.0f - cameraFollow_) + Vector3::Normalized(toTarget) * cameraFollow_;
	}

	// 向き→オイラー角。レールカメラの「接線→オイラー角」と同じ逆算式
	// (回転規約: 行ベクトル×Rx→Ry→Rz で forward = (cos(pitch)·sin(yaw), -sin(pitch), cos(pitch)·cos(yaw)))
	const float yaw = std::atan2(look.x, look.z);
	const float pitch = std::atan2(-look.y, std::sqrt(look.x * look.x + look.z * look.z));
	outRotate = { pitch, yaw, 0.0f }; // バンク(roll)はしない(レールカメラと同じ)
}

void GameOverSequence::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Game Over");
	const int phaseIndex = static_cast<int>(phase_);
	ImGui::Text("Phase: %s (%.2f s)", phaseIndex < static_cast<int>(std::size(kPhaseNames)) ? kPhaseNames[phaseIndex] : "?",
		phaseTimer_);
	ImGui::Text("Time Scale: %.2f", timeScale_);
	// ImGuiのフォントは日本語の字形を持っていないので英語で書く
	ImGui::TextDisabled("Play: Hierarchy > Force GameOver / Skip: Enter or START");

	ImGui::SeparatorText("Timing (real time)");
	ImGui::DragFloat("Impact", &params_.impactDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::DragFloat("Spin", &params_.spinDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::DragFloat("Drift", &params_.driftDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::DragFloat("Explode", &params_.explodeDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Fade", &params_.fadeDuration, 0.01f, 0.1f, 5.0f, "%.2f s");

	ImGui::SeparatorText("Time Scale");
	ImGui::SliderFloat("Slow", &params_.slowTimeScale, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Recover", &params_.recoverDuration, 0.01f, 0.0f, 3.0f, "%.2f s");

	ImGui::SeparatorText("Motion (game time)");
	ImGui::DragFloat("Start Speed", &params_.startSpeed, 0.1f, 0.0f, 50.0f, "%.1f m/s");
	ImGui::DragFloat("Drift Accel", &params_.driftAcceleration, 0.1f, 0.0f, 100.0f, "%.1f m/s^2");
	ImGui::DragFloat("Fall Accel", &params_.fallAcceleration, 0.1f, 0.0f, 30.0f, "%.1f m/s^2");
	ImGui::DragFloat("Roll Speed", &params_.rollSpeed, 0.1f, 0.0f, 50.0f, "%.1f rad/s");
	ImGui::DragFloat("Tumble Speed", &params_.tumbleSpeed, 0.01f, -5.0f, 5.0f, "%.2f rad/s");
	ImGui::SliderFloat("Camera Follow", &params_.cameraFollow, 0.0f, 1.0f, "%.2f");

	ImGui::SeparatorText("Shake");
	ImGui::DragFloat("Impact Shake", &params_.impactShake, 0.05f, 0.0f, 5.0f, "%.2f m");
	ImGui::DragFloat("Explosion Shake", &params_.explosionShake, 0.05f, 0.0f, 5.0f, "%.2f m");

	ImGui::SeparatorText("Screen");
	ImGui::SliderFloat("Impact Vignette", &params_.impactVignette, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Flash Time", &params_.flashDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::SliderFloat("Dark Vignette", &params_.darkVignette, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Impact Blur", &params_.impactBlur, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Impact Blur Time", &params_.impactBlurDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::SliderFloat("Spin Blur", &params_.spinBlur, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Explosion Blur", &params_.explosionBlur, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Explosion Blur Time", &params_.explosionBlurDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::SliderFloat("Drain Monochrome", &params_.drainMonochrome, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Desaturate", &params_.desaturateRatio, 0.05f, 1.0f, "%.2f");

	ImGui::SeparatorText("Particles");
	ImGui::DragFloat("Smoke Rate", &params_.smokeRate, 1.0f, 0.0f, 500.0f, "%.0f /s");
	ImGui::DragFloat("Spark Rate", &params_.sparkRate, 1.0f, 0.0f, 500.0f, "%.0f /s");
	ImGui::DragInt("Impact Sparks", &params_.impactSparkCount, 0.5f, 0, 300);
	ImGui::DragInt("Explosion", &params_.explosionCount, 0.5f, 0, 500);
	ImGui::DragInt("Explosion Sparks", &params_.explosionSparkCount, 0.5f, 0, 300);
	ImGui::DragInt("Explosion Smoke", &params_.explosionSmokeCount, 0.5f, 0, 300);
	if (ImGui::TreeNode("Smoke Config")) {
		ParticleManager::DrawConfigImGui(smokeEmitter_->GetConfig());
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Spark Config")) {
		ParticleManager::DrawConfigImGui(sparkEmitter_->GetConfig());
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Explosion Config")) {
		ParticleManager::DrawConfigImGui(explosionConfig_);
		ImGui::TreePop();
	}
	ImGui::End();
#endif
}
