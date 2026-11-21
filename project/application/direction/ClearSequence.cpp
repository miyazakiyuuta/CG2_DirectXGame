#include "direction/ClearSequence.h"

#include "3d/Camera.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "effect/EffectManager.h"
#include "effect/ParticleManager.h"
#include "effect/RadialBlur.h"
#include "effect/Vignette.h"
#include "math/Easing.h"
#include "utility/Random.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// パーティクルのグループ名。登録(RegisterEffect)・発生・後片付け(ClearParticles)で同じ名前を使う。
	// ゲームオーバー演出とは別のグループにする: 同時には起きないが、見た目の調整を互いに巻き込まないため
	const std::string kFireEffect = "clearFire";
	const std::string kSparkEffect = "clearSpark";
	const std::string kSmokeEffect = "clearSmoke";
	// 粒の画像はブレンドの仕方で使い分ける(GameOverSequenceと同じ)。
	// circle.png は黒背景に白い丸の加算用、smoke.png はαに丸の形を持たせたαブレンド用
	const std::string kAdditiveTexture = "resources/circle.png";
	const std::string kSmokeTexture = "resources/smoke.png";

	// 粒の設定(fireConfig_など)を決めたときの女王の判定半径[m]。実際の半径との比で粒を拡大する
	constexpr float kReferenceRadius = 6.0f;

	// 亡骸の色。光っている間はライティングを切り、この色をそのまま出す(陰影が無くなり、光って見える)
	const Vector4 kFlashColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 断末魔の始め。撃破の直前の色(HPの残量の色=赤)に近づけて、入れ替わりを目立たせない
	const Vector4 kDyingColorStart = { 1.0f, 0.3f, 0.25f, 1.0f };
	// 大爆発の直前。内側から焼けていくように、明るい橙へ寄せていく
	const Vector4 kDyingColorEnd = { 1.0f, 0.75f, 0.45f, 1.0f };

	// Vignetteの形。被弾演出・ゲームオーバー演出と同じ形にそろえる
	constexpr float kVignetteRadius = 0.22f;
	constexpr float kVignetteSoftness = 0.55f;
	constexpr float kVignettePower = 1.4f;
	// 画面の中心まで閉じたときの半径。radius+softness が0未満になると、画面全体が色(ここでは白)になる
	constexpr float kClosedRadius = -(kVignetteSoftness + 0.05f);

	// RadialBlurの形(Startでまとめて設定する。RadialBlurは全シーン共有なので、使う側が毎回すべて決める)
	constexpr float kBlurWidth = 0.015f;
	constexpr int kBlurSamples = 12;
	constexpr float kBlurFocusRadius = 0.12f; // 女王は自機より大きく映るので、鮮明に残す範囲も広めにする
	constexpr float kBlurFocusSoftness = 0.35f;

	// ImGui表示用の段階名(Phaseの並びと揃える)
	const char* const kPhaseNames[] = { "None", "Impact", "Convulse", "Burst", "Exterminate", "Hold", "Finished" };

	// 名前でポストエフェクトを引いて具象型にする。登録が外れていてもnullptrを返すだけで落とさない
	// (演出は無くてもゲームは成立する)
	template <class T>
	T* FindPostEffect(EffectManager* effectManager, const char* name) {
		if (!effectManager) {
			return nullptr;
		}
		return static_cast<T*>(effectManager->FindEffect(name));
	}

	// 経過時間 → 進み具合(0〜1)。長さが0以下でも割り算で落ちないよう、そのときは「もう終わった」扱いにする
	float Progress(float time, float duration) {
		return duration > 0.0f ? std::clamp(time / duration, 0.0f, 1.0f) : 1.0f;
	}

	Vector4 LerpColor(const Vector4& a, const Vector4& b, float t) {
		return { std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t), std::lerp(a.w, b.w, t) };
	}
}

ClearSequence::ClearSequence() = default;

ClearSequence::~ClearSequence() = default;

void ClearSequence::Initialize(EffectManager* effectManager) {
	effectManager_ = effectManager;

	// 女王の亡骸の器。モデルはStartで女王と同じものを入れる(女王の種別はデータ側で変わりうるため)
	remains_ = std::make_unique<Object3d>();
	remains_->Initialize(Object3dCommon::GetInstance());

	// 炎。ゲームオーバーの爆発(自機・幅約1m)よりずっと大きく速くする:
	// 女王は幅12mあり、カメラから約60m先にいるので、同じ大きさの炎では小さな火にしか見えない
	fireConfig_.minScale = { 2.5f, 2.5f, 2.5f };
	fireConfig_.maxScale = { 6.0f, 6.0f, 6.0f };
	fireConfig_.minVelocity = { -22.0f, -22.0f, -22.0f };
	fireConfig_.maxVelocity = { 22.0f, 22.0f, 22.0f };
	fireConfig_.acceleration = { 0.0f, 2.0f, 0.0f }; // 熱で少しだけ昇る
	fireConfig_.lifeTimeMin = 0.5f;
	fireConfig_.lifeTimeMax = 1.3f;
	fireConfig_.startColor = { 1.0f, 0.95f, 0.7f, 1.0f };
	fireConfig_.endColor = { 0.9f, 0.25f, 0.0f, 0.0f };
	fireConfig_.endScaleRatio = 0.3f;
	ParticleManager::GetInstance()->RegisterEffect(kFireEffect, kAdditiveTexture, fireConfig_, BlendMode::Add);

	// 火花。速く遠くまで飛び、重力で落ちながら黄色→橙へ冷えて消える
	sparkConfig_.minScale = { 0.25f, 0.25f, 0.25f };
	sparkConfig_.maxScale = { 0.5f, 0.5f, 0.5f };
	sparkConfig_.minVelocity = { -30.0f, -10.0f, -30.0f };
	sparkConfig_.maxVelocity = { 30.0f, 35.0f, 30.0f };
	sparkConfig_.acceleration = { 0.0f, -25.0f, 0.0f };
	sparkConfig_.lifeTimeMin = 0.4f;
	sparkConfig_.lifeTimeMax = 1.0f;
	sparkConfig_.startColor = { 1.0f, 1.0f, 0.75f, 1.0f };
	sparkConfig_.endColor = { 1.0f, 0.4f, 0.05f, 0.0f };
	sparkConfig_.endScaleRatio = 0.4f;
	ParticleManager::GetInstance()->RegisterEffect(kSparkEffect, kAdditiveTexture, sparkConfig_, BlendMode::Add);

	// 煙。炎より長く残し、全滅と余韻の間の背景になる。光らないので加算ではなくαブレンド
	smokeConfig_.minScale = { 3.0f, 3.0f, 3.0f };
	smokeConfig_.maxScale = { 5.0f, 5.0f, 5.0f };
	smokeConfig_.minVelocity = { -6.0f, -2.0f, -6.0f };
	smokeConfig_.maxVelocity = { 6.0f, 6.0f, 6.0f };
	smokeConfig_.lifeTimeMin = 1.5f;
	smokeConfig_.lifeTimeMax = 2.8f;
	smokeConfig_.startColor = { 0.5f, 0.48f, 0.45f, 0.6f };
	smokeConfig_.endColor = { 0.15f, 0.15f, 0.15f, 0.0f };
	smokeConfig_.endScaleRatio = 2.5f; // 広がりながら薄くなる
	ParticleManager::GetInstance()->RegisterEffect(kSmokeEffect, kSmokeTexture, smokeConfig_, BlendMode::Alpha);
}

void ClearSequence::Start(const std::string& modelPath, const Transform& transform, const Vector3& center, float radius) {
	// 亡骸のモデルは女王と同じもの(EnemySpawnerが読み込み済み)。前回と同じなら差し替えない
	if (modelPath != remainsModelPath_) {
		ModelManager::GetInstance()->LoadModel(modelPath);
		remains_->SetModel(modelPath);
		remainsModelPath_ = modelPath;
	}
	baseTransform_ = transform;
	center_ = center;
	radius_ = (std::max)(radius, 0.01f);
	elapsed_ = 0.0f;
	sinceBurst_ = -1.0f;
	popTimer_ = params_.popIntervalStart;
	flashTimer_ = params_.whiteFlashDuration; // 撃破の瞬間、亡骸を白く光らせる

	EnterPhase(Phase::Impact);
	timeScale_ = ComputeTimeScale();

	// このフレームの描画に間に合わせる(女王が消えたフレームから、同じ姿の亡骸が見えているように)
	UpdateRemains(0.0f, 0.0f);

	// 撃破の瞬間の火花。爆発の中心(判定の球の中心)から全方向へ飛ばし、表面を突き抜けて飛び散らせる
	// (ヒットストップの間も粒は等速で流れる。止まった画面の中で火花だけが散る)
	ParticleManager::GetInstance()->Emit(kSparkEffect, center_, ScaleToRemains(sparkConfig_),
		static_cast<uint32_t>((std::max)(params_.impactSparkCount, 0)));

	// RadialBlurの形を決め、撃破の瞬間だけ強く掛ける(Triggerは強さを一瞬上げて自動で戻す)
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

void ClearSequence::Stop() {
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	elapsed_ = 0.0f;
	sinceBurst_ = -1.0f;
	timeScale_ = 1.0f;
	flashTimer_ = 0.0f;
	popTimer_ = 0.0f;

	// 時間倍率はParticleManagerの全グループに掛かる。戻さないと撃破の爆発や次のシーンの粒までスローのままになる
	ParticleManager::GetInstance()->SetTimeScale(1.0f);
	// 粒はワールド座標で残り続けるので、Edit・リトライ・結果画面へ持ち越さないよう消す
	ParticleManager::GetInstance()->ClearParticles(kFireEffect);
	ParticleManager::GetInstance()->ClearParticles(kSparkEffect);
	ParticleManager::GetInstance()->ClearParticles(kSmokeEffect);

	// ポストエフェクトは全シーン共有なので、掛けたまま残さない。
	// Vignetteは被弾演出と共有だが、そちらは毎フレームON/OFFを決め直すので、ここで切っても困らない
	if (Vignette* vignette = FindPostEffect<Vignette>(effectManager_, "Vignette")) {
		vignette->enabled = false;
	}
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		radialBlur->enabled = false;
		radialBlur->SetIntensity(0.0f); // 常時掛けの強さはシーンをまたいで残るので0へ戻す
	}
}

void ClearSequence::Skip() {
	// 撃破の瞬間(0.5秒)と大爆発の閃光は飛ばさせない: 短いうえに、ここを飛ばすと「何が起きたか」が伝わらない。
	// 断末魔の途中なら、その場で大爆発へ進める(亡骸が消えずに結果へ飛ぶと、話がつながらない)。
	// 大爆発より後(全滅・余韻)は、もう見せたいものを見せ終えているので終わりへ進める
	if (phase_ == Phase::Convulse) {
		EnterPhase(Phase::Burst);
	} else if (phase_ == Phase::Exterminate || phase_ == Phase::Hold) {
		EnterPhase(Phase::Finished);
	}
}

void ClearSequence::Update(float deltaTime, const Camera& camera) {
	// 終わった後は何も触らない。シーンが結果画面への遷移を頼むと、ChangeSceneが全ポストエフェクトを切る。
	// ここで点け直すと、次のシーンへ点いたまま渡ってしまう(GameOverSequenceと同じ注意)
	if (phase_ == Phase::None || phase_ == Phase::Finished) {
		return;
	}

	// 段階を進める。長さはすべて実時間なので、スローモーションの間も演出の長さは変わらない
	phaseTimer_ += deltaTime;
	elapsed_ += deltaTime;
	if (sinceBurst_ >= 0.0f) {
		sinceBurst_ += deltaTime;
	}
	switch (phase_) {
	case Phase::Impact:
		if (phaseTimer_ >= params_.impactDuration) {
			EnterPhase(Phase::Convulse);
		}
		break;
	case Phase::Convulse:
		if (phaseTimer_ >= params_.convulseDuration) {
			EnterPhase(Phase::Burst);
		}
		break;
	case Phase::Burst:
		if (phaseTimer_ >= params_.burstDuration) {
			EnterPhase(Phase::Exterminate);
		}
		break;
	case Phase::Exterminate:
		if (phaseTimer_ >= params_.exterminateDuration) {
			EnterPhase(Phase::Hold);
		}
		break;
	case Phase::Hold:
		if (phaseTimer_ >= params_.holdDuration) {
			EnterPhase(Phase::Finished);
		}
		break;
	default:
		break;
	}

	// ゲーム時間の倍率。シーンは次のフレームから、自機・敵・弾のdeltaTimeにもこの倍率を掛ける
	timeScale_ = ComputeTimeScale();
	// 粒にも同じ倍率を掛け、断末魔の間は小爆発もゆっくり広がる。ただしヒットストップ(倍率0)の間は等速のまま流す:
	// 止まった画面の中で火花だけが散る見せ方にするため(ゲームオーバー演出・通常の被弾と同じ)
	ParticleManager::GetInstance()->SetTimeScale(phase_ == Phase::Impact ? 1.0f : timeScale_);

	// 小爆発をカメラ側へ寄せるため、今の視点を控える
	cameraPosition_ = camera.GetTranslate();

	if (IsRemainsVisible()) {
		UpdateRemains(deltaTime, deltaTime * timeScale_);
	}
	UpdatePops(deltaTime);

	// 終わったフレームも、その時点の状態をここで1度だけ書き込む(次のフレームからは上の早期returnで触らない)
	ApplyPostEffects(camera);
}

void ClearSequence::EnterPhase(Phase phase) {
	phase_ = phase;
	phaseTimer_ = 0.0f;

	if (phase == Phase::Burst) {
		Burst();
	}
}

float ClearSequence::ComputeTimeScale() const {
	switch (phase_) {
	case Phase::Impact:
		return 0.0f; // ヒットストップ
	case Phase::Convulse:
		return params_.slowTimeScale;
	default:
		// 大爆発で等速へ戻す。ゲームオーバー演出のようになめらかに戻さないのは、
		// 爆発の勢いで「止まっていた時間が一気に動き出す」ように見せたいため
		return 1.0f;
	}
}

float ClearSequence::GetConvulseProgress() const {
	switch (phase_) {
	case Phase::None:
	case Phase::Impact:
		return 0.0f;
	case Phase::Convulse:
		return Progress(phaseTimer_, params_.convulseDuration);
	default:
		return 1.0f;
	}
}

bool ClearSequence::IsRemainsVisible() const {
	return phase_ == Phase::Impact || phase_ == Phase::Convulse;
}

void ClearSequence::UpdateRemains(float deltaTime, float gameDeltaTime) {
	const float progress = GetConvulseProgress();

	Transform transform = baseTransform_;
	// 膨らみ: 始めはほとんど変わらず、大爆発の直前に一気に膨らむ(InCubic)。内側から圧が高まっていく見え方にする
	const float scaleRatio = 1.0f + params_.swell * Easing::InCubic(progress);
	transform.scale *= scaleRatio;
	// 震え: 断末魔の間だけ、位置を毎フレーム乱数でずらす。振れ幅は女王の大きさに比例させ、だんだん激しくする。
	// 撃破の瞬間(ヒットストップ)は止めておく: 「止まった」ことが伝わらなくなるため
	if (phase_ == Phase::Convulse) {
		const float amplitude = radius_ * std::lerp(params_.jitterStart, params_.jitterEnd, progress);
		transform.translate += {
			Random::GetFloat(-amplitude, amplitude),
			Random::GetFloat(-amplitude, amplitude),
			Random::GetFloat(-amplitude, amplitude) };
	}
	remains_->GetTransform() = transform;

	// 光る: 撃破の瞬間と、小爆発のたび。ライティングを切ると、陰影の無いその色そのもの(=光って見える)になる
	// (夜のステージでは、ライティングありの白い面は月明かり程度にしか明るくならないため)
	flashTimer_ = (std::max)(flashTimer_ - deltaTime, 0.0f);
	const bool flashing = flashTimer_ > 0.0f;
	remains_->SetEnableLighting(!flashing);
	remains_->SetColor(flashing ? kFlashColor : LerpColor(kDyingColorStart, kDyingColorEnd, progress));

	remains_->Update(gameDeltaTime);
}

void ClearSequence::UpdatePops(float deltaTime) {
	if (phase_ != Phase::Convulse) {
		return;
	}
	popTimer_ -= deltaTime;
	// 1フレームに2回以上来ることもある(間隔が短いとき・フレームが重いとき)ので、間隔の分だけ繰り返す
	while (popTimer_ <= 0.0f) {
		Pop();
		// 間隔は今の進み具合で決める。断末魔の始めは間をあけ、大爆発へ向けて詰めていく
		const float interval = std::lerp(params_.popIntervalStart, params_.popIntervalEnd, GetConvulseProgress());
		popTimer_ += (std::max)(interval, 0.01f); // 0にすると無限に繰り返すので下限を置く
	}
}

void ClearSequence::Pop() {
	// 置き場所: 爆発の中心(判定の球の中心)から、乱数の向きをカメラ側へ寄せた向きに、判定半径の少し外側。
	// 全方向に散らすと半分以上が体の裏側に出て亡骸に隠れるので、カメラから見える側へ寄せる
	Vector3 direction = {
		Random::GetFloat(-1.0f, 1.0f), Random::GetFloat(-1.0f, 1.0f), Random::GetFloat(-1.0f, 1.0f) };
	const Vector3 toCamera = cameraPosition_ - center_;
	if (toCamera.LengthSquared() > 1.0e-6f) {
		direction += Vector3::Normalized(toCamera);
	}
	if (direction.LengthSquared() < 1.0e-6f) {
		direction = { 0.0f, 1.0f, 0.0f };
	}
	const Vector3 position = center_ + Vector3::Normalized(direction) * (radius_ * params_.popSpread);

	// 大爆発の炎と火花を小さく・遅く・短くしたもの(見た目の調整を大爆発と共有する)
	ParticleConfig fire = ScaleToRemains(fireConfig_);
	fire.minScale *= 0.35f;
	fire.maxScale *= 0.35f;
	fire.minVelocity *= 0.3f;
	fire.maxVelocity *= 0.3f;
	fire.lifeTimeMin *= 0.6f;
	fire.lifeTimeMax *= 0.6f;
	ParticleConfig spark = ScaleToRemains(sparkConfig_);
	spark.minVelocity *= 0.5f;
	spark.maxVelocity *= 0.5f;

	ParticleManager* particleManager = ParticleManager::GetInstance();
	particleManager->Emit(kFireEffect, position, fire, static_cast<uint32_t>((std::max)(params_.popFireCount, 0)));
	particleManager->Emit(kSparkEffect, position, spark, static_cast<uint32_t>((std::max)(params_.popSparkCount, 0)));

	flashTimer_ = params_.popFlashDuration; // 小爆発のたびに亡骸を一瞬光らせる
	++popCount_; // 揺れはシーンが前後の差で起こす
}

void ClearSequence::Burst() {
	ParticleManager* particleManager = ParticleManager::GetInstance();
	const Vector3& center = center_;

	// 炎・火花・煙を爆発の中心から一斉に出す。亡骸はこの段階から描かないので、体の内側から出しても隠れない
	particleManager->Emit(kFireEffect, center, ScaleToRemains(fireConfig_),
		static_cast<uint32_t>((std::max)(params_.burstFireCount, 0)));
	particleManager->Emit(kSparkEffect, center, ScaleToRemains(sparkConfig_),
		static_cast<uint32_t>((std::max)(params_.burstSparkCount, 0)));
	particleManager->Emit(kSmokeEffect, center, ScaleToRemains(smokeConfig_),
		static_cast<uint32_t>((std::max)(params_.burstSmokeCount, 0)));

	// 白い閃光と黒い縁が引いていく時計を始める(ApplyPostEffectsが使う)
	sinceBurst_ = 0.0f;

	// RadialBlurを一瞬強める(中心は爆発した点。ApplyPostEffectsが毎フレーム合わせる)
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		radialBlur->Trigger(params_.burstBlur, params_.burstBlurDuration);
	}
	// 画面の揺れと残りの敵の全滅はシーンが持っているので、シーンが段階の変化(Burstに入った)を見て起こす
}

void ClearSequence::ApplyPostEffects(const Camera& camera) {
	// --- 白: 撃破の瞬間の白い縁と、大爆発の白い閃光 ---
	// 撃破の白い縁は、演出が始まってからの実時間で抜く(ヒットストップの間も同じ長さで消える)
	const float impactWhite = params_.impactFlash * (1.0f - Progress(elapsed_, params_.impactFlashDuration));
	float burstWhite = 0.0f;
	// --- 黒い縁: 断末魔の間に濃くして緊張を高め、大爆発の後にゆっくり引く ---
	float dark = 0.0f;
	if (sinceBurst_ >= 0.0f) {
		// 閃光は OutCubic で引く: 一瞬画面を真っ白に飛ばし、すぐに爆炎が見え始め、薄い明るさだけが少し残る
		burstWhite = 1.0f - Easing::OutCubic(Progress(sinceBurst_, params_.burstFlashDuration));
		dark = params_.darkVignette * (1.0f - Easing::InOutSine(Progress(sinceBurst_, params_.darkReleaseDuration)));
	} else if (phase_ == Phase::Convulse) {
		dark = params_.darkVignette * Easing::InOutSine(GetConvulseProgress());
	}

	if (Vignette* vignette = FindPostEffect<Vignette>(effectManager_, "Vignette")) {
		// Vignetteは色を1つしか持てないので、白と黒は「濃い方の強さ」と「そのうち白の割合」で1つの色にまとめる。
		// 閃光が引くにつれて白の割合が下がり、残っている黒い縁へなめらかに移る
		const float white = (std::max)(impactWhite, burstWhite);
		const float intensity = (std::max)(white, dark);
		const float whiteRatio = intensity > 0.0f ? white / intensity : 0.0f;
		vignette->enabled = intensity > 0.0f;
		vignette->SetColor({ whiteRatio, whiteRatio, whiteRatio });
		vignette->SetIntensity(intensity);
		// 大爆発の白は、半径を縮めて画面の中心まで閉じる(ゲームオーバー演出の暗転と同じやり方を、白で使う)。
		// 新しいシェーダを足さずに「画面全体を白く飛ばす」ことができる
		vignette->SetRadius(std::lerp(kVignetteRadius, kClosedRadius, burstWhite));
		vignette->SetSoftness(kVignetteSoftness);
		vignette->SetPower(kVignettePower);
		vignette->SetAspect(1.0f);
	}

	// --- RadialBlur: 女王(大爆発の後は爆発した点)を中心に放射状にぼかす ---
	if (RadialBlur* radialBlur = FindPostEffect<RadialBlur>(effectManager_, "RadialBlur")) {
		// 常時掛けの強さ。断末魔の間、大爆発へ向けて強めていく。
		// 瞬間的な強さ(撃破・大爆発)はTriggerが上乗せして自動で戻すので、ここは土台だけ決める
		const float baseIntensity = phase_ == Phase::Convulse ? params_.convulseBlur * GetConvulseProgress() : 0.0f;
		// 全滅の間までは点けておく(大爆発の瞬間の強さが抜け切るまで)。余韻に入ったら外す
		radialBlur->enabled = phase_ == Phase::Impact || phase_ == Phase::Convulse ||
			phase_ == Phase::Burst || phase_ == Phase::Exterminate;
		radialBlur->SetIntensity(baseIntensity);
		radialBlur->SetCenterWorld(center_, camera.GetViewProjectionMatrix());
	}
}

ParticleConfig ClearSequence::ScaleToRemains(const ParticleConfig& config) const {
	const float ratio = radius_ / kReferenceRadius;
	ParticleConfig scaled = config;
	scaled.minScale *= ratio;
	scaled.maxScale *= ratio;
	scaled.minVelocity *= ratio;
	scaled.maxVelocity *= ratio;
	scaled.acceleration *= ratio;
	return scaled;
}

void ClearSequence::Draw() {
	if (IsRemainsVisible()) {
		remains_->Draw();
	}
}

void ClearSequence::SetCamera(Camera* camera) {
	remains_->SetCamera(camera);
}

void ClearSequence::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Clear");
	const int phaseIndex = static_cast<int>(phase_);
	ImGui::Text("Phase: %s (%.2f s)", phaseIndex < static_cast<int>(std::size(kPhaseNames)) ? kPhaseNames[phaseIndex] : "?",
		phaseTimer_);
	ImGui::Text("Time Scale: %.2f  Pops: %d", timeScale_, popCount_);
	// ImGuiのフォントは日本語の字形を持っていないので英語で書く
	ImGui::TextDisabled("Play: Hierarchy > Force Clear (Play) / Preview Clear (Edit)");
	ImGui::TextDisabled("Skip: Enter or START");

	ImGui::SeparatorText("Timing (real time)");
	ImGui::DragFloat("Impact", &params_.impactDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Convulse", &params_.convulseDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::DragFloat("Burst", &params_.burstDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Exterminate", &params_.exterminateDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::DragFloat("Hold", &params_.holdDuration, 0.01f, 0.0f, 5.0f, "%.2f s");

	ImGui::SeparatorText("Time Scale");
	ImGui::SliderFloat("Slow", &params_.slowTimeScale, 0.0f, 1.0f, "%.2f");

	ImGui::SeparatorText("Remains");
	ImGui::SliderFloat("Swell", &params_.swell, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Jitter Start", &params_.jitterStart, 0.0f, 0.3f, "%.3f");
	ImGui::SliderFloat("Jitter End", &params_.jitterEnd, 0.0f, 0.3f, "%.3f");
	ImGui::DragFloat("White Flash", &params_.whiteFlashDuration, 0.005f, 0.0f, 1.0f, "%.3f s");
	ImGui::DragFloat("Pop Flash", &params_.popFlashDuration, 0.005f, 0.0f, 0.5f, "%.3f s");

	ImGui::SeparatorText("Pops");
	ImGui::DragFloat("Interval Start", &params_.popIntervalStart, 0.005f, 0.01f, 2.0f, "%.3f s");
	ImGui::DragFloat("Interval End", &params_.popIntervalEnd, 0.005f, 0.01f, 2.0f, "%.3f s");
	ImGui::SliderFloat("Spread", &params_.popSpread, 0.0f, 2.0f, "%.2f");

	ImGui::SeparatorText("Shake");
	ImGui::DragFloat("Impact Shake", &params_.impactShake, 0.05f, 0.0f, 5.0f, "%.2f m");
	ImGui::DragFloat("Pop Shake", &params_.popShake, 0.01f, 0.0f, 2.0f, "%.2f m");
	ImGui::DragFloat("Burst Shake", &params_.burstShake, 0.05f, 0.0f, 5.0f, "%.2f m");

	ImGui::SeparatorText("Screen");
	ImGui::SliderFloat("Impact Flash", &params_.impactFlash, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Impact Flash Time", &params_.impactFlashDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::SliderFloat("Dark Vignette", &params_.darkVignette, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Burst Flash Time", &params_.burstFlashDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Dark Release Time", &params_.darkReleaseDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
	ImGui::SliderFloat("Impact Blur", &params_.impactBlur, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Impact Blur Time", &params_.impactBlurDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::SliderFloat("Convulse Blur", &params_.convulseBlur, 0.0f, 1.0f, "%.2f");
	ImGui::SliderFloat("Burst Blur", &params_.burstBlur, 0.0f, 1.0f, "%.2f");
	ImGui::DragFloat("Burst Blur Time", &params_.burstBlurDuration, 0.01f, 0.0f, 2.0f, "%.2f s");

	ImGui::SeparatorText("Particles");
	ImGui::DragInt("Impact Sparks", &params_.impactSparkCount, 0.5f, 0, 300);
	ImGui::DragInt("Pop Fire", &params_.popFireCount, 0.5f, 0, 200);
	ImGui::DragInt("Pop Sparks", &params_.popSparkCount, 0.5f, 0, 200);
	ImGui::DragInt("Burst Fire", &params_.burstFireCount, 0.5f, 0, 600);
	ImGui::DragInt("Burst Sparks", &params_.burstSparkCount, 0.5f, 0, 600);
	ImGui::DragInt("Burst Smoke", &params_.burstSmokeCount, 0.5f, 0, 300);
	if (ImGui::TreeNode("Fire Config")) {
		ParticleManager::DrawConfigImGui(fireConfig_);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Spark Config")) {
		ParticleManager::DrawConfigImGui(sparkConfig_);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Smoke Config")) {
		ParticleManager::DrawConfigImGui(smokeConfig_);
		ImGui::TreePop();
	}
	ImGui::End();
#endif
}
