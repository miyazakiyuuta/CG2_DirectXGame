#include "scene/TitleScene.h"

#include "base/DirectXCommon.h"
#include "base/WinApp.h"
#include "io/Input.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "math/Easing.h"
#include "math/Matrix4x4.h"
#include "math/mathFunction.h"
#include "player/Player.h"
#include "scene/SceneManager.h"
#include "transition/BlindTransition.h"
#include "effect/EffectManager.h"
#include "effect/Vignette.h"
#include "effect/ParticleManager.h"
#include "utility/Random.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	const std::string kTitleTexture = "resources/ui/title.png";
	const std::string kStartTexture = "resources/ui/start.png";
	// 操作説明はゲーム中のHUDと同じ画像を使う(表記が食い違わないように1枚で持つ)
	const std::string kControlsTexture = "resources/ui/controls.png";

	// 画像は1280x720想定で作ってあるので、実解像度に合わせて拡大する。
	// kClientWidthは構成で変わる(Debug=1920x1080 / Release=1280x720)ため直値は使えない
	constexpr float kBaseWidth = 1280.0f;

	// --- カメラ ---
	// ゲーム本編(stage.jsonのcamera)と同じ距離・画角にして、自機の見え方を揃える。
	// 自機の真後ろから定位置を見つめるので、定位置の自機は画面のちょうど中央に来る
	constexpr float kCameraBackDistance = 12.0f;
	constexpr float kCameraFovY = 0.45f;
	// 自機より少し上から見下ろす(約7度)。翼は厚さ2cmほどの薄い板なので、
	// 同じ高さから見ると真横から見ることになり、静止した瞬間に翼がほとんど消えてしまう
	constexpr float kCameraHeight = 1.5f;

	// --- 自機 ---
	// 定位置(画面中央)
	const Vector3 kShipHomePosition = { 0.0f, 0.0f, 0.0f };
	// カメラの定位置と向き。自機の定位置の後方やや上から、定位置を見下ろす。
	// 回転規約では正のピッチが下向きなので、高さと距離の比からそのまま角度を出せる
	const Vector3 kCameraPosition = kShipHomePosition + Vector3{ 0.0f, kCameraHeight, -kCameraBackDistance };
	const Vector3 kCameraRotate = { std::atan2(kCameraHeight, kCameraBackDistance), 0.0f, 0.0f };
	// 上昇の開始位置。定位置の奥行きでは画面の下端がおよそ y=-2.9m なので、そのすぐ外から出す。
	// 上昇は速さ0から加速するので、遠くから始めると画面外で加速している間だけ何も映らない時間ができる
	const Vector3 kShipStartPosition = { 0.0f, -3.3f, 0.0f };
	// 中央へ戻る時間のうち、左への傾きを解き切るまでの割合。
	// 移動より先に傾きを戻すことで、最も速く動く中盤には進む向き(右)へ傾けられる
	constexpr float kReturnUnbankPortion = 0.4f;
	// 中央へ戻る途中で、進む向き(右)へ傾ける量[rad]
	constexpr float kReturnCounterBank = 0.3f;

	// --- 岩 ---
	const std::string kRockModel = "boulder/boulder.obj";
	// 直径 約4.5m(自機の幅1.1mの4倍ほど)
	constexpr float kRockScale = 3.0f;
	// モデルの原点は底面にあり、中心は原点から高さ0.67(モデル単位)の所にある。
	// 回転の軸を岩の中心にするため、回転後の中心のずれを位置で打ち消すのに使う
	constexpr float kRockPivotHeight = 0.67f;
	// 出現位置。画面では右上の奥にあたる
	const Vector3 kRockStartPosition = { 9.0f, 5.0f, 50.0f };
	// 岩が狙う点。自機の定位置から右下へ約1.4mずらしてある(岩の半径は約2.3mなので、
	// 自機がよけなければ当たる軌道のまま)。
	// 定位置そのものを狙うと、すれ違った後にカメラの手前を左へ流れ、大きな岩がよけた自機に
	// 画面上で重なってしまう(黒背景では奥行きが分かりにくく、ぶつかったように見える)。
	// 下へずらすと、岩はすれ違った後に画面の下へ抜けていく。
	// 値はカメラ・回避量(dodgeDistance)と合わせて、画面上で自機と重ならないことを確かめてある
	const Vector3 kRockAimPosition = { 1.0f, -1.0f, 0.0f };
	// この奥行きより手前に来たら消す。既定の軌道ではz=-8.9付近で画面の下へ抜け切っている
	// (岩を大きくすると抜け切るのが遅くなるので、そのときはここも手前へずらす。
	// 抜け切る前に消すと、画面の下端に一部が残ったまま岩が消えて見える)
	constexpr float kRockHideZ = -9.5f;
	// 転がる速さ[rad/s](軸ごとに変えて、同じ面ばかり見えないようにする)
	const Vector3 kRockSpinSpeed = { 2.1f, 1.3f, 0.8f };
	// 出始めにディザで溶かしながら出す時間[秒](何も無い所に突然現れないように)
	constexpr float kRockFadeInDuration = 0.3f;
	// モデル単位での岩の半径(kRockScale=3で直径約4.5m)。隕石の半径[m]から拡大率を逆算するのに使う
	constexpr float kRockModelRadius = 0.75f;

	// --- 出発の演出 ---
	// 出発の演出で真っ黒になった後に進むシーン。ゲームとしては"GAMEPLAY"。
	// 学内の画面遷移コンテストのデモ中は"TITLE"にして、タイトルからタイトルへ切り替える
	// (新しいタイトルはオープニングから始まるので、遷移を何度でも見せられる)
	const std::string kDepartNextScene = "GAMEPLAY";
	// 開始入力の時に飛んでいたタイトルの岩を、ディザで消し切るまでの時間[秒]。
	// 右へよける自機と重なったり、隕石と紛れたりしないよう、すぐに片付ける
	constexpr float kOldRockFadeOutDuration = 0.2f;
	// 自機の幅(約1.1m)の半分。隕石との間隔を確かめるときに、自機を球とみなす半径として使う
	constexpr float kShipHalfWidth = 0.55f;
	// 隕石が見える画面上の位置。画面の縦の半分を1とした単位(上端が1、横は±アスペクト比まで)。
	// 中央やや左上 = 自機の左上をかすめる向き。隕石はカメラへ一直線に進むので、
	// 画面上の位置は最後までここから動かず、大きくなるだけ(迫ってくる物は画面上で止まって見える)
	constexpr float kMeteorScreenX = -0.55f;
	constexpr float kMeteorScreenY = 0.3f;
	// 隕石が現れる距離[m](カメラの描画距離100mより手前)
	constexpr float kMeteorStartDistance = 80.0f;
	// 赤熱した色。マテリアル色はテクスチャとライトの結果に掛かるので、緑と青を落として橙に寄せる
	const Vector4 kMeteorColor = { 1.0f, 0.55f, 0.35f, 1.0f };
	// 転がる速さ[rad/s]。大きな物がゆっくり回ると重く見えるので、タイトルの岩より遅くする
	const Vector3 kMeteorSpinSpeed = { 0.9f, 0.5f, 0.3f };
	// 炎の尾。粒を出す円の半径は隕石の半径と同じにして、輪郭から出す
	// (内側から出すと、手前の岩肌に隠れてほとんど見えない)
	const std::string kTrailEffectName = "meteorTrail";
	const std::string kTrailTexture = "resources/circle.png";
	// プレビュー(Debug)で真っ黒になってからReadyへ戻るまでの時間[秒]
	constexpr float kPreviewBlackHold = 0.5f;

	// --- 文言 ---
	// 作品名の透明度を上げ切るまでの時間[秒]。大きさの変化(titleInDuration)より短くして、
	// 迫ってくる途中からはっきり見えるようにする
	constexpr float kTitleFadeDuration = 0.4f;
	// 開始案内の明滅の速さ[rad/s]。結果画面のリトライ案内と同じ周期
	constexpr float kBlinkSpeed = 4.0f;
	// 操作説明の濃さ(開始案内より目立たないよう少し透かす。HUDと同じ濃さ)と、フェードインの時間[秒]
	constexpr float kControlsAlpha = 0.75f;
	constexpr float kControlsFadeDuration = 0.4f;
	// 片付けるとき、開始案内・操作説明が消え切るまでの時間[秒]。
	// 作品名が上へ抜け切る(titleOutDuration)より先に消して、画面に文字が残らないようにする
	constexpr float kTextFadeOutDuration = 0.5f;

	// --- サウンド ---
	// タイトルでだけ流すBGM
	const std::string kBgmSound = "resources/sounds/title_bgm.mp3";
	// BGMのカテゴリ音量。ゲーム本編(GamePlaySceneのkBgmVolume)と揃える。
	// タイトルは最初のシーンで、まだ誰もカテゴリ音量を設定していないため、
	// ここで決めないと初期値の1.0(本編の10倍)で鳴ってしまう
	constexpr float kBgmVolume = 0.10f;
	// SEのカテゴリ音量。BGMと同じ理由で、ゲーム本編(GamePlaySceneのkSeVolume)と揃える
	constexpr float kSeVolume = 0.30f;
	// 隕石が視野を埋めた(画面が真っ黒になった)瞬間の音。専用の素材が無いので撃破音を流用する。
	// 画面には何も映らない瞬間なので、音だけで「ぶつかった」ことを伝えるよう大きく鳴らす
	const std::string kFillSound = "resources/sounds/explosion.mp3";
	constexpr float kFillVolume = 1.0f;

	// フェーズ内の進行率(0〜1)。長さ0以下は即完了とする
	// (ImGuiで長さを0にされても、0除算で壊れないようにするため)
	float Progress(float timer, float duration) {
		return duration > 0.0f ? std::clamp(timer / duration, 0.0f, 1.0f) : 1.0f;
	}

	// よける動きの進行率(0〜1)。岩が自機に届く時刻(reachTime)のleadTime秒前によけ始め、durationかけてよけ切る。
	// 岩の速さを変えても、よけるタイミングが自動で追従する
	float DodgeProgress(float time, float reachTime, float leadTime, float duration) {
		return Progress(time - (reachTime - leadTime), duration);
	}

	// 画面上の位置(縦の半分を1とする単位。横は±アスペクト比まで)を通る視線の向き(ワールド空間の単位ベクトル)。
	// カメラから見た向き(奥行き1の所で、縦の半分の長さがtan(画角/2)になる位置)を作り、カメラの向きで回す。
	// 揺れを足す前の定位置の向きを使うので、画面が揺れても隕石の通り道は動かない
	Vector3 ScreenRayDirection(float x, float y) {
		const float tanHalfFov = std::tan(kCameraFovY * 0.5f);
		const Vector3 local = { x * tanHalfFov, y * tanHalfFov, 1.0f };
		return Vector3::Normalized(Matrix4x4::Rotate(kCameraRotate).Transform(local));
	}

	// カメラから隕石の中心への向き。隕石はこの向きの逆へ(カメラへ)一直線に進むので、最後まで変わらない
	Vector3 MeteorDirection() {
		return ScreenRayDirection(kMeteorScreenX, kMeteorScreenY);
	}

#ifdef USE_IMGUI
	// ImGui表示用のフェーズ名(Phaseの並びと揃える)
	const char* const kPhaseNames[] = { "Rise", "Hold", "RockPass", "Return", "TitleIn", "Ready", "TitleOut", "Depart" };
#endif
}

TitleScene::TitleScene() = default;

void TitleScene::Initialize() {
	const float uiScale = static_cast<float>(WinApp::kClientWidth) / kBaseWidth;
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float height = static_cast<float>(WinApp::kClientHeight);

	// --- カメラ ---
	// 定位置から動かさない(出発の演出で岩が止まるときだけ、定位置を中心に揺らす)
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetFovY(kCameraFovY);
	camera_->SetTranslate(kCameraPosition);
	camera_->SetRotate(kCameraRotate);

	// --- 自機と岩 ---
	// 自機のモデルと大きさはPlayerから取る(ゲーム本編と見た目を1箇所で揃える)
	ModelManager::GetInstance()->LoadModel(Player::ModelPath());
	shipObject_ = std::make_unique<Object3d>();
	shipObject_->Initialize(Object3dCommon::GetInstance());
	shipObject_->SetModel(Player::ModelPath());
	shipObject_->SetScale({ Player::ModelScale(), Player::ModelScale(), Player::ModelScale() });
	shipObject_->SetCamera(camera_.get());

	ModelManager::GetInstance()->LoadModel(kRockModel);
	rockObject_ = std::make_unique<Object3d>();
	rockObject_->Initialize(Object3dCommon::GetInstance());
	rockObject_->SetModel(kRockModel);
	rockObject_->SetScale({ kRockScale, kRockScale, kRockScale });
	rockObject_->SetCamera(camera_.get());

	// 出発の演出の隕石。モデルはタイトルの岩と同じで、大きさ(半径から逆算)はImGuiで変えられるよう毎フレーム決める
	meteorObject_ = std::make_unique<Object3d>();
	meteorObject_->Initialize(Object3dCommon::GetInstance());
	meteorObject_->SetModel(kRockModel);
	meteorObject_->SetColor(kMeteorColor);
	meteorObject_->SetCamera(camera_.get());

	// 隕石の炎の尾。加算合成にして、重なるほど明るく光らせる。
	// RegisterEffectは冪等なので、タイトルへ戻ってきてシーンが作り直されても二重登録にならない
	trailConfig_.minScale = { 0.8f, 0.8f, 0.8f };
	trailConfig_.maxScale = { 1.6f, 1.6f, 1.6f };
	trailConfig_.minVelocity = { -1.5f, -1.5f, -1.5f }; // 外向きの速さ(trailSpreadSpeed)に足す揺らぎ
	trailConfig_.maxVelocity = { 1.5f, 1.5f, 1.5f };
	trailConfig_.lifeTimeMin = 0.25f;
	trailConfig_.lifeTimeMax = 0.5f;
	trailConfig_.startColor = { 1.0f, 0.85f, 0.4f, 1.0f }; // 生まれた直後は黄色く、
	trailConfig_.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };    // 冷えるにつれ赤くなって消える
	trailConfig_.endScaleRatio = 0.3f;
	ParticleManager::GetInstance()->RegisterEffect(kTrailEffectName, kTrailTexture, trailConfig_, BlendMode::Add);
	// ParticleManagerは渡されたカメラで粒を描く。外すのはFinalize(このシーンのカメラと一緒に消えるため)
	ParticleManager::GetInstance()->SetCamera(camera_.get());

	// --- 文言 ---
	// Sprite::InitializeはTextureManagerに登録済みのSRVを引くだけなので、先に読み込む
	TextureManager::GetInstance()->LoadTexture(kTitleTexture);
	TextureManager::GetInstance()->LoadTexture(kStartTexture);
	TextureManager::GetInstance()->LoadTexture(kControlsTexture);

	// 上から作品名 → 開始案内 → 操作説明。アンカーを中心にすると画像の実寸に関係なく中央へ揃い、
	// 作品名を拡大縮小したときも中心が動かない
	titleSprite_ = std::make_unique<Sprite>();
	titleSprite_->Initialize(SpriteCommon::GetInstance(), kTitleTexture);
	titleSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	// Initialize直後のサイズ=テクスチャの実寸(AdjustTextureSize)。それを解像度倍率で拡大する
	const Vector2 titleTextureSize = titleSprite_->GetSize();
	titleSize_ = { titleTextureSize.x * uiScale, titleTextureSize.y * uiScale };
	titleSprite_->SetSize(titleSize_);
	titlePos_ = { centerX, height * 0.36f };
	titleSprite_->SetPos(titlePos_);

	startSprite_ = std::make_unique<Sprite>();
	startSprite_->Initialize(SpriteCommon::GetInstance(), kStartTexture);
	startSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 startSize = startSprite_->GetSize();
	startSprite_->SetSize({ startSize.x * uiScale, startSize.y * uiScale });
	startSprite_->SetPos({ centerX, height * 0.62f });

	controlsSprite_ = std::make_unique<Sprite>();
	controlsSprite_->Initialize(SpriteCommon::GetInstance(), kControlsTexture);
	controlsSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 controlsSize = controlsSprite_->GetSize();
	controlsSprite_->SetSize({ controlsSize.x * uiScale, controlsSize.y * uiScale });
	controlsSprite_->SetPos({ centerX, height * 0.84f });

	// --- サウンド ---
	// BGMはモデルやテクスチャの読み込みを終えた最後に鳴らし始める
	// (先に鳴らすと、読み込みで最初の描画が遅れる間に音だけが先に進んでしまう)。止めるのはFinalize
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::BGM, kBgmVolume);
	SoundManager::GetInstance()->SetCategoryVolume(SoundManager::SoundCategory::SE, kSeVolume);
	fillSound_ = SoundManager::GetInstance()->LoadFile(kFillSound);
	bgmSound_ = SoundManager::GetInstance()->LoadFile(kBgmSound);
	bgmHandle_ = SoundManager::GetInstance()->PlayWave(
		bgmSound_, true, SoundManager::SoundCategory::BGM);

	// 演出は最初(自機の上昇)から始める
	ChangePhase(Phase::Rise);
}

void TitleScene::Finalize() {
	// パーティクルに渡したカメラはこのシーンと一緒に消えるので、参照を外しておく
	// (ParticleManagerはFrameworkが毎フレーム更新するため、残すと解放済みのカメラを読みに行く)。
	// 生き残っている炎の尾の粒はこのシーンの座標で出したものなので、
	// 消しておかないと、次のシーンのカメラで描かれて次の画面に混ざる(黒帯が開いていく間に見えてしまう)
	ParticleManager::GetInstance()->ClearParticles(kTrailEffectName);
	ParticleManager::GetInstance()->SetCamera(nullptr);

	// 次のシーンへ鳴り続けないよう止める。
	// ゲーム開始時はフェードアウトを終えて止まっていることが多く、その場合StopWaveは何もしない
	SoundManager::GetInstance()->StopWave(bgmHandle_);
	bgmHandle_ = 0;
}

void TitleScene::Update(float deltaTime) {
	// 出発の演出中は入力を受けない(演出の途中でやり直したり、二重にシーンを切り替えたりしないように)
	if (phase_ != Phase::Depart && IsStartPressed()) {
		if (titleShown_) {
			// 作品名を一度見せた後は、どのフェーズでも即出発する
			// (開始案内は1周に数秒しか出ないので、消えている間に押されても反応させる)
			StartDepart(false);
		} else {
			// 最初の演出中の入力はスキップ(完成状態へ飛ぶ)。同じ押下でゲームまで進めると
			// 開始案内を見る前に始まってしまうので、開始はもう一度押したときにする
			ChangePhase(Phase::Ready);
		}
	}

	UpdateOpening(deltaTime);
	UpdateDarkness();

	// 隕石が自機を追い越すときの揺れ(風圧)。カメラの定位置に、カメラから見た右/上方向のずれを足す。
	// 残り時間に比例して振れ幅を減らす(GamePlaySceneの被弾の揺れと同じ作り)
	Vector3 cameraPosition = kCameraPosition;
	if (shakeTimer_ > 0.0f) {
		shakeTimer_ = (std::max)(shakeTimer_ - deltaTime, 0.0f);
		const float amplitude = departParams_.shakeStrength * Progress(shakeTimer_, departParams_.shakeDuration);
		const Vector3 offset = { Random::GetFloat(-amplitude, amplitude), Random::GetFloat(-amplitude, amplitude), 0.0f };
		cameraPosition += Matrix4x4::Rotate(kCameraRotate).Transform(offset);
	}
	camera_->SetTranslate(cameraPosition);
	camera_->Update();
	camera_->TransferToGPU();

	// 機首は常に奥(+Z)向き。傾き(ロール)だけをフェーズごとに変える
	shipObject_->SetTranslate(shipPosition_);
	// 先にモデルの機首を+Z(画面の奥)へ向け直し(Player::ModelCorrection)、そのあと機首の軸まわりに傾ける
	shipObject_->SetRotate(Matrix4x4::ToEuler(Player::ModelCorrection() * Matrix4x4::RotateZ(shipRoll_)));
	shipObject_->Update(deltaTime);
	if (rockVisible_) {
		rockObject_->Update(deltaTime);
	}
	if (meteorVisible_) {
		meteorObject_->Update(deltaTime);
	}

	UpdateSprites();
}

void TitleScene::UpdateOpening(float deltaTime) {
	phaseTimer_ += deltaTime;

	// 岩と隕石はRockPass・Departの間だけ出す。他のフェーズへ飛んだとき(スキップ・再生し直し)に残らないよう、
	// 毎フレーム一旦消してから各フェーズで決め直す
	rockVisible_ = false;
	meteorVisible_ = false;

	switch (phase_) {
	case Phase::Rise: {
		// 画面の下のすぐ外から中央へ。InOutSineなので速さ0からゆっくり動き出し、
		// 中盤で最も速くなってから減速して定位置に収まる
		// (動き出しの瞬間に最高速が出る曲線だと、機体がいきなり弾かれたような機械的な動きになる)
		const float t = Progress(phaseTimer_, params_.riseDuration);
		shipPosition_ = Lerp(kShipStartPosition, kShipHomePosition, Easing::InOutSine(t));
		shipRoll_ = 0.0f;
		if (t >= 1.0f) {
			ChangePhase(Phase::Hold);
		}
		break;
	}
	case Phase::Hold:
		shipPosition_ = kShipHomePosition;
		shipRoll_ = 0.0f;
		if (phaseTimer_ >= params_.holdDuration) {
			ChangePhase(Phase::RockPass);
		}
		break;
	case Phase::RockPass: {
		// 左へよけながら左へ傾ける(Z軸の正回転で右翼が上がり、左翼が下がる)。
		// 止まっていた機体が急に動き出さないよう、位置も傾きも速さ0から加速する曲線にする
		const float dodge = GetDodgeProgress();
		const float dodgeEased = Easing::InOutSine(dodge);
		shipPosition_ = kShipHomePosition + Vector3{ -params_.dodgeDistance * dodgeEased, 0.0f, 0.0f };
		shipRoll_ = params_.dodgeBank * dodgeEased;

		rockVisible_ = UpdateRock(*rockObject_, kRockStartPosition, kRockAimPosition, params_.rockSpeed, phaseTimer_);
		// 岩が抜け切り、よけ終わってもいたら次へ(よけている途中で戻り始めて位置が飛ばないよう両方を待つ)
		if (!rockVisible_ && dodge >= 1.0f) {
			ChangePhase(Phase::Return);
		}
		break;
	}
	case Phase::Return: {
		// 岩が抜けてひと呼吸おいてから中央へ戻る(returnDelayの間は進行率が0のまま)
		const float t = Progress(phaseTimer_ - params_.returnDelay, params_.returnDuration);
		shipPosition_ = kShipHomePosition +
			Vector3{ -params_.dodgeDistance * (1.0f - Easing::InOutCubic(t)), 0.0f, 0.0f };
		// 左への傾きは前半(kReturnUnbankPortion)で解き、移動が最も速い中盤では進む向き(右)へ少し傾ける。
		// 左に傾いたまま右へ滑ると、横から押し流されているように見えるため。
		// どちらも回り始めと終わりの速さが0になる形にして(sinの2乗は両端で変化が0)、急に回り出さないようにする
		const float counterBank = std::sin(std::numbers::pi_v<float> * t);
		shipRoll_ = params_.dodgeBank * (1.0f - Easing::InOutSine(t / kReturnUnbankPortion)) -
			kReturnCounterBank * counterBank * counterBank;
		if (t >= 1.0f) {
			ChangePhase(Phase::TitleIn);
		}
		break;
	}
	case Phase::TitleIn:
		shipPosition_ = kShipHomePosition;
		shipRoll_ = 0.0f;
		if (phaseTimer_ >= params_.titleInDuration) {
			ChangePhase(Phase::Ready);
		}
		break;
	case Phase::Ready:
		// 演出の完成状態(スキップ時もここへ飛ぶ)。開始入力はUpdate側で見る
		shipPosition_ = kShipHomePosition;
		shipRoll_ = 0.0f;
		if (phaseTimer_ >= params_.readyDuration) {
			ChangePhase(Phase::TitleOut);
		}
		break;
	case Phase::TitleOut:
		// 文言を片付ける(動きはUpdateSprites側)。片付け終わったら、次の岩を呼んで演出をくり返す。
		// RockPassは経過時間0から岩の位置・回転・よけるタイミングを求め直すので、毎回同じ動きになる
		shipPosition_ = kShipHomePosition;
		shipRoll_ = 0.0f;
		if (phaseTimer_ >= params_.titleOutDuration) {
			ChangePhase(Phase::RockPass);
		}
		break;
	case Phase::Depart:
		UpdateDepart(deltaTime);
		break;
	}
}

bool TitleScene::UpdateRock(Object3d& rock, const Vector3& start, const Vector3& aim, float speed, float time) {
	// 奥から、狙う点(自機の定位置の少し下)をめがけて等速で直進する。
	// 岩自身は加減速しないが、遠近法で近づくほど画面上では速く大きく見える
	const Vector3 direction = Vector3::Normalized(aim - start);
	const Vector3 center = start + direction * (speed * time);
	if (center.z < kRockHideZ) {
		return false;
	}

	// 転がしながら飛ばす。モデルの原点は底面にあるので、そのまま回すと底を軸に振り回される。
	// 回転後の「原点→中心」のずれを求め、その分だけ位置を引いて中心を軌道に乗せる
	const Vector3 rotate = kRockSpinSpeed * time;
	const Vector3 pivotOffset =
		Matrix4x4::Rotate(rotate).Transform({ 0.0f, kRockPivotHeight * kRockScale, 0.0f });
	rock.SetRotate(rotate);
	rock.SetTranslate(center - pivotOffset);
	// 出始めはディザ(Object3dのdissolve)で溶かしながら出す
	rock.SetDissolve(1.0f - Progress(time, kRockFadeInDuration));
	return true;
}

float TitleScene::GetDodgeProgress() const {
	// 岩が狙う点(=自機の位置の奥行き)に届く時刻から逆算し、そのdodgeLeadTime秒前によけ始める
	const float reachTime = (kRockAimPosition - kRockStartPosition).Length() / params_.rockSpeed;
	return DodgeProgress(phaseTimer_, reachTime, params_.dodgeLeadTime, params_.dodgeDuration);
}

void TitleScene::StartDepart(bool preview) {
	departPreview_ = preview;

	// 今の見た目を記録する。Departではここを始点に動かすので、どのフェーズで押されても位置や文言が飛ばない
	departStart_.shipPosition = shipPosition_;
	departStart_.shipRoll = shipRoll_;
	// rockVisible_は前フレームの結果(=今画面に映っている岩)
	departStart_.rockTime = (phase_ == Phase::RockPass && rockVisible_) ? phaseTimer_ : -1.0f;
	// 文言はDrawと同じ条件で「今描いているか」を見る
	departStart_.titleVisible = phase_ == Phase::TitleIn || phase_ == Phase::Ready || phase_ == Phase::TitleOut;
	departStart_.guideVisible = phase_ == Phase::Ready || phase_ == Phase::TitleOut;
	departStart_.titlePos = titleSprite_->GetPos();
	departStart_.titleSize = titleSprite_->GetSize();
	departStart_.titleAlpha = titleSprite_->GetColor().w;
	departStart_.startAlpha = startSprite_->GetColor().w;
	departStart_.controlsAlpha = controlsSprite_->GetColor().w;
	trailTimer_ = 0.0f;

	ChangePhase(Phase::Depart);

	if (!preview) {
		// 画面が真っ黒になる時刻にBGMも消え切るようにする(消え切るとSoundManagerが自動で止める)。
		// 黒くなった瞬間に音だけがぶつ切りになると、暗転の余韻が台無しになるため
		SoundManager::GetInstance()->FadeOut(bgmHandle_, GetFillTime());
	}
}

void TitleScene::UpdateDepart(float deltaTime) {
	const float time = phaseTimer_;
	const float prevTime = time - deltaTime;
	const float passTime = GetMeteorPassTime();
	const float fillTime = GetFillTime();

	// --- 自機 ---
	// 隕石が自機を追い越す時刻から逆算して、右へよけ切っておく(タイトルの左よけの左右反転)。
	// 始点は開始入力の瞬間の位置・傾きなので、左へよけている途中で押されてもそこから右へ動く。
	// 傾きは正で左なので、右へ傾けるときは負
	const float dodge = Easing::InOutSine(
		DodgeProgress(time, passTime, departParams_.dodgeLeadTime, departParams_.dodgeDuration));
	const Vector3 dodgedPosition = kShipHomePosition + Vector3{ params_.dodgeDistance, 0.0f, 0.0f };
	shipPosition_ = Lerp(departStart_.shipPosition, dodgedPosition, dodge);
	shipRoll_ = departStart_.shipRoll + (-params_.dodgeBank - departStart_.shipRoll) * dodge;

	// --- 開始入力の時に飛んでいたタイトルの岩 ---
	// 軌道はそのまま続け、ディザで素早く消す(急に消すと、何も無い所で岩がふっと消えたように見える)
	if (departStart_.rockTime >= 0.0f && time < kOldRockFadeOutDuration) {
		const float rockTime = departStart_.rockTime + time;
		rockVisible_ = UpdateRock(*rockObject_, kRockStartPosition, kRockAimPosition, params_.rockSpeed, rockTime);
		// 出始めのディザ(UpdateRockが設定)とどちらか濃く消える方を使い、出始めで押されても急に濃くならないようにする
		const float fadeIn = 1.0f - Progress(rockTime, kRockFadeInDuration);
		rockObject_->SetDissolve((std::max)(fadeIn, Progress(time, kOldRockFadeOutDuration)));
	}

	// --- 隕石 ---
	// 視野を埋めた後(画面は真っ黒)は尾を出さない。出した粒はシーンを抜けるときに消すが、無駄に増やさないため
	UpdateMeteor(time, deltaTime, time < fillTime);

	// 隕石が自機を追い越した瞬間(前フレームから今フレームの間にその時刻をまたいだ)に、風圧で画面を揺らす
	if (prevTime < passTime && time >= passTime) {
		shakeTimer_ = departParams_.shakeDuration;
	}

	// --- 隕石が視野を埋めて、画面が真っ黒になったら ---
	if (prevTime < fillTime && time >= fillTime) {
		SoundManager* soundManager = SoundManager::GetInstance();
		const SoundManager::SoundHandle handle = soundManager->PlayWave(fillSound_);
		soundManager->SetVolume(handle, kFillVolume);
	}
	if (departPreview_) {
		// プレビューはシーンを切り替えず、真っ黒を少し見せてから開始案内の画面へ戻る
		if (time >= fillTime + kPreviewBlackHold) {
			ChangePhase(Phase::Ready);
		}
	} else if (!sceneChangeRequested_ && time >= fillTime) {
		// 画面はこのシーンが真っ黒にし終えているので、帯は閉じた状態から始めて、すぐに開く側へ進める
		sceneChangeRequested_ = true;
		SceneManager::GetInstance()->ChangeScene(
			kDepartNextScene, std::make_unique<BlindTransition>(BlindTransition::StartState::Closed));
	}
}

void TitleScene::UpdateMeteor(float time, float deltaTime, bool emitTrail) {
	// カメラからの距離を、出現位置から速さ×時間だけ縮める。向きは変えない(カメラへ一直線)
	const Vector3 direction = MeteorDirection();
	const float distance = kMeteorStartDistance - departParams_.meteorSpeed * time;
	// カメラを通り過ぎたら消す(プレビューで真っ黒を見せている間にだけ起こる)
	meteorVisible_ = distance > 0.0f;
	if (!meteorVisible_) {
		return;
	}
	const Vector3 center = kCameraPosition + direction * distance;

	// 半径からモデルの拡大率を決め、タイトルの岩と同じく中心を軸に転がす
	const float radius = departParams_.meteorRadius;
	const float scale = radius / kRockModelRadius;
	const Vector3 rotate = kMeteorSpinSpeed * time;
	const Vector3 pivotOffset =
		Matrix4x4::Rotate(rotate).Transform({ 0.0f, kRockPivotHeight * scale, 0.0f });
	meteorObject_->SetScale({ scale, scale, scale });
	meteorObject_->SetRotate(rotate);
	meteorObject_->SetTranslate(center - pivotOffset);
	meteorObject_->SetDissolve(1.0f - Progress(time, kRockFadeInDuration));

	if (!emitTrail || departParams_.trailRate <= 0.0f) {
		return;
	}
	// --- 炎の尾 ---
	// 輪郭の円周上の、ばらばらの位置から粒を出し、外向きの速さを持たせる。
	// 粒は隕石の速さを受け継がずその場に残るので、隕石がカメラへ進むほど後ろへ取り残され、輪郭から流れる尾になる。
	// 円はカメラから見た右/上の2軸で張る(隕石はほぼ正面にあるので、カメラから見た輪郭とほぼ一致する)
	const Matrix4x4 cameraRotation = Matrix4x4::Rotate(kCameraRotate);
	const Vector3 right = cameraRotation.Transform({ 1.0f, 0.0f, 0.0f });
	const Vector3 up = cameraRotation.Transform({ 0.0f, 1.0f, 0.0f });
	const float interval = 1.0f / departParams_.trailRate;
	trailTimer_ += deltaTime;
	while (trailTimer_ >= interval) {
		trailTimer_ -= interval;
		const float angle = Random::GetFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
		const Vector3 outward = right * std::cos(angle) + up * std::sin(angle);
		// 粒ごとに外向きの速さを足す(登録したconfigの速さは、そこへ足す揺らぎとして使う)
		ParticleConfig config = trailConfig_;
		const Vector3 spread = outward * departParams_.trailSpreadSpeed;
		config.minVelocity += spread;
		config.maxVelocity += spread;
		ParticleManager::GetInstance()->Emit(kTrailEffectName, center + outward * radius, config, 1);
	}
}

float TitleScene::GetFillTime() const {
	// 隕石を半径rの球とみなす。中心までの距離がd、中心の向きuと視線の向きcのなす角がθのとき、
	// 視線と中心の距離はd·sinθなので、球がその視線を覆うのは d·sinθ ≤ r のとき。
	// 画面の4隅の視線をすべて覆えば、球の輪郭(凸)の内側に画面の四角形がまるごと入る。
	// 隕石はカメラへ一直線に進みuが変わらないので、4隅のsinθの最大値から、埋まる距離が1つの式で決まる
	const Vector3 direction = MeteorDirection();
	const float aspect = static_cast<float>(WinApp::kClientWidth) / static_cast<float>(WinApp::kClientHeight);
	float maxSin = 0.0f;
	for (float x : { -aspect, aspect }) {
		for (float y : { -1.0f, 1.0f }) {
			// 単位ベクトル同士の外積の長さ = sinθ
			const float sinTheta = Vector3::Cross(direction, ScreenRayDirection(x, y)).Length();
			maxSin = (std::max)(maxSin, sinTheta);
		}
	}
	// でこぼこで輪郭が欠ける分、半径を少し小さく見積もる(=少し近づいてから埋まったことにする)
	const float coverRadius = departParams_.meteorRadius * departParams_.silhouetteRatio;
	const float fillDistance = coverRadius / maxSin;
	return (kMeteorStartDistance - fillDistance) / departParams_.meteorSpeed;
}

float TitleScene::GetMeteorPassTime() const {
	// 自機の定位置を隕石の通り道(カメラからの直線)へ投影した点が、通り道のうち自機に最も近い点。
	// その点までのカメラからの距離 = (定位置 - カメラ)と通り道の向きの内積
	const float passDistance = Vector3::Dot(kShipHomePosition - kCameraPosition, MeteorDirection());
	return (kMeteorStartDistance - passDistance) / departParams_.meteorSpeed;
}

float TitleScene::GetMeteorClearance() const {
	// 右へよけた後の自機と、隕石の通り道(カメラを通る直線)との距離から、隕石の半径と自機の半幅を引いた残り。
	// 点と直線の距離 = |(点 - 直線上の1点) × 直線の向き|(向きは単位ベクトル)
	const Vector3 dodgedPosition = kShipHomePosition + Vector3{ params_.dodgeDistance, 0.0f, 0.0f };
	const float lineDistance = Vector3::Cross(dodgedPosition - kCameraPosition, MeteorDirection()).Length();
	return lineDistance - departParams_.meteorRadius - kShipHalfWidth;
}

void TitleScene::UpdateDarkness() {
	// 次のシーンへの切り替えを頼んだ後は触らない。ChangeSceneが全ポストエフェクトを切っており
	// (そのフレームから帯が画面を覆っている)、ここで点け直すとVignetteが次のシーンへ点いたまま渡ってしまう
	if (sceneChangeRequested_) {
		return;
	}
	IPostEffect* effect = GetEffectManager()->FindEffect("Vignette");
	if (!effect) {
		return; // 登録が外れていても落とさない(暗転が無くても遷移は成立する)
	}

	// 視野が埋まるdarkDuration秒前から暗くし始め、埋まる瞬間に真っ黒にする。
	// InCubicで初めはゆっくり、最後に一気に暗くして、隕石が自機を追い越す場面を暗さで隠さないようにする
	float darkness = 0.0f;
	if (phase_ == Phase::Depart) {
		const float darkStart = GetFillTime() - departParams_.darkDuration;
		if (phaseTimer_ >= darkStart) {
			darkness = Easing::InCubic(Progress(phaseTimer_ - darkStart, departParams_.darkDuration));
		}
	}
	// 暗くしないフェーズでは切っておく(プレビューの後やReplayで演出を戻したときに残らないように)
	effect->SetEnabled(darkness > 0.0f);
	if (!effect->IsEnabled()) {
		return;
	}

	// 画面全体を一様に暗くする。Vignetteは中心からの距離がradius+softnessより外側を色(黒)に寄せるので、
	// radius+softnessを負にすると全画素が「外側」になり、強さ(intensity)がそのまま画面全体の暗さになる
	// (出力 = 元の色 × (1 - intensity))
	Vignette* vignette = static_cast<Vignette*>(effect);
	vignette->SetColor({ 0.0f, 0.0f, 0.0f });
	vignette->SetIntensity(darkness);
	vignette->SetRadius(-2.0f);
	vignette->SetSoftness(1.0f);
	vignette->SetPower(1.0f);
	vignette->SetAspect(1.0f);
}

void TitleScene::UpdateSprites() {
	// 作品名: 大きさ0から等倍へ広げ、奥から迫ってきたように見せる。
	// OutBackで一度少し大きく行き過ぎてから収まるので、手前で止まった「着地感」が出る。
	// 透明度は出だしの短い間に上げ切る(点から急に現れないようにしつつ、迫る途中からはっきり見せる)
	Vector2 titleSize = titleSize_;
	float titleAlpha = 1.0f;
	Vector2 titlePos = titlePos_;
	if (phase_ == Phase::TitleIn) {
		const float titleScale = Easing::OutBack(Progress(phaseTimer_, params_.titleInDuration));
		titleSize = { titleSize_.x * titleScale, titleSize_.y * titleScale };
		titleAlpha = Easing::OutCubic(Progress(phaseTimer_, kTitleFadeDuration));
	} else if (phase_ == Phase::TitleOut) {
		// 片付け: 定位置から、作品名の高さぶん画面の上の外まで動かす(中心アンカーなので全体が画面外へ出る)。
		// InCubicで速さ0からゆっくり動き出し、加速したまま抜けていく
		const float out = Easing::InCubic(Progress(phaseTimer_, params_.titleOutDuration));
		titlePos.y = titlePos_.y + (-titleSize_.y - titlePos_.y) * out;
	} else if (phase_ == Phase::Depart) {
		// 出発: TitleOutと同じ動きで、開始入力の瞬間の位置・大きさ・濃さから上へ抜ける
		// (迫り出す途中や上へ抜ける途中で押されても、そこから続けて動かす)
		const float out = Easing::InCubic(Progress(phaseTimer_, params_.titleOutDuration));
		titleSize = departStart_.titleSize;
		titleAlpha = departStart_.titleAlpha;
		titlePos = departStart_.titlePos;
		titlePos.y += (-titleSize.y - departStart_.titlePos.y) * out;
	}
	// 位置は毎フレーム決め直す(上へ抜けた後、次に迫り出すときは定位置へ戻っている)
	titleSprite_->SetPos(titlePos);
	titleSprite_->SetSize(titleSize);
	titleSprite_->SetColor({ 1.0f, 1.0f, 1.0f, titleAlpha });

	// 開始案内と操作説明は作品名が出揃ってから出し、片付けでその場で消す(それ以外のフェーズでは描かない)。
	// 明滅は従来と同じ周期・振れ幅で、位相だけずらして暗い側から始める(パッと出ないように)
	if (phase_ == Phase::Ready || phase_ == Phase::TitleOut) {
		// 表示し始めてからの時刻。片付け中もReadyの続きから数えて、境目で明滅の濃さが飛ばないようにする
		float shownTime = phaseTimer_;
		// 消え残っている割合。片付け中は1から0へ減らし、明滅の濃さに掛けてその場で消す
		float remain = 1.0f;
		if (phase_ == Phase::TitleOut) {
			shownTime = params_.readyDuration + phaseTimer_;
			remain = 1.0f - Easing::InOutSine(Progress(phaseTimer_, kTextFadeOutDuration));
		}
		const float startAlpha = (0.55f - 0.45f * std::cos(shownTime * kBlinkSpeed)) * remain;
		startSprite_->SetColor({ 1.0f, 1.0f, 1.0f, startAlpha });
		const float controlsAlpha =
			kControlsAlpha * Easing::OutCubic(Progress(shownTime, kControlsFadeDuration)) * remain;
		controlsSprite_->SetColor({ 1.0f, 1.0f, 1.0f, controlsAlpha });
	} else if (phase_ == Phase::Depart) {
		// 出発: 開始入力の瞬間の濃さから、TitleOutと同じ時間でその場で消す(明滅はやめ、押したことが伝わるようにする)
		const float remain = 1.0f - Easing::InOutSine(Progress(phaseTimer_, kTextFadeOutDuration));
		startSprite_->SetColor({ 1.0f, 1.0f, 1.0f, departStart_.startAlpha * remain });
		controlsSprite_->SetColor({ 1.0f, 1.0f, 1.0f, departStart_.controlsAlpha * remain });
	}

	// 頂点と行列の書き込みはUpdateが行うため、Drawの前に毎フレーム呼ぶ
	titleSprite_->Update();
	startSprite_->Update();
	controlsSprite_->Update();
}

void TitleScene::Draw() {
	// 先に3D(自機・岩・隕石)を描き、その上に文言のスプライトを重ねる。背景はシーンRTのクリア色(黒)のまま。
	// 隕石は自機と同じカメラ・深度バッファで描くので、自機を追い越した後は手前の隕石が自機を正しく覆い隠す。
	// 炎の尾(パーティクル)は、Frameworkがシーンの描画の後に描く
	shipObject_->Draw();
	if (rockVisible_) {
		rockObject_->Draw();
	}
	if (meteorVisible_) {
		meteorObject_->Draw();
	}

	// スプライトは深度OFF・αブレンドON。出していない(片付け終わった)文言は描かない。
	// 出発の演出中は、開始入力の瞬間に描いていたものだけを片付け終わるまで描く
	const bool departing = phase_ == Phase::Depart;
	SpriteCommon::GetInstance()->CommonDrawSetting();
	if (phase_ == Phase::TitleIn || phase_ == Phase::Ready || phase_ == Phase::TitleOut ||
		(departing && departStart_.titleVisible)) {
		titleSprite_->Draw();
	}
	if (phase_ == Phase::Ready || phase_ == Phase::TitleOut || (departing && departStart_.guideVisible)) {
		startSprite_->Draw();
		controlsSprite_->Draw();
	}
}

void TitleScene::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Title");
	// 今どのフェーズにいて、入ってから何秒か(演出の進み具合の確認用)
	ImGui::Text("Phase: %s (%.2f s)", kPhaseNames[static_cast<int>(phase_)], phaseTimer_);
	// 今、開始入力が「スキップ」と「ゲーム開始」のどちらとして扱われるか
	ImGui::Text("Start Input: %s", titleShown_ ? "start game" : "skip");
	// 出発の演出中は、Replay・Preview Departとも押せなくする
	// (ゲーム開始の演出の途中で戻すと、BGMだけ消えたタイトルが残ってしまう)
	const bool departing = phase_ == Phase::Depart;
	ImGui::BeginDisabled(departing);
	// 調整した値をすぐ確かめられるよう、演出を最初から再生し直す
	// (作品名を見せたかどうかも戻し、最初の演出中のスキップの挙動まで含めて確かめられるようにする)
	if (ImGui::Button("Replay")) {
		titleShown_ = false;
		ChangePhase(Phase::Rise);
	}
	ImGui::SameLine();
	// 出発の演出だけを再生する。シーンを切り替えずに真っ黒からReadyへ戻るので、調整のたびにゲームへ飛ばない
	if (ImGui::Button("Preview Depart")) {
		StartDepart(true);
	}
	ImGui::EndDisabled();

	ImGui::SeparatorText("Ship");
	ImGui::DragFloat("Rise", &params_.riseDuration, 0.01f, 0.0f, 10.0f, "%.2f s");
	ImGui::DragFloat("Hold", &params_.holdDuration, 0.01f, 0.0f, 10.0f, "%.2f s");

	ImGui::SeparatorText("Rock / Dodge");
	// 速さ0だと岩が永遠に届かず演出が止まるので、手入力でも下限を割らないようにする
	ImGui::DragFloat("Rock Speed", &params_.rockSpeed, 0.5f, 5.0f, 200.0f, "%.1f m/s",
		ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Dodge Lead", &params_.dodgeLeadTime, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Dodge Time", &params_.dodgeDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Dodge Distance", &params_.dodgeDistance, 0.05f, 0.0f, 10.0f, "%.2f m");
	ImGui::SliderAngle("Dodge Bank", &params_.dodgeBank, 0.0f, 90.0f);
	ImGui::DragFloat("Return Delay", &params_.returnDelay, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Return Time", &params_.returnDuration, 0.01f, 0.0f, 3.0f, "%.2f s");

	ImGui::SeparatorText("Title Logo");
	ImGui::DragFloat("Title In", &params_.titleInDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Ready", &params_.readyDuration, 0.01f, 0.0f, 10.0f, "%.2f s");
	ImGui::DragFloat("Title Out", &params_.titleOutDuration, 0.01f, 0.0f, 3.0f, "%.2f s");

	// 上と同じ名前の項目(Dodge Lead等)は、IDに"##Depart"を付けて区別する(ImGuiは同じIDの項目を1つとして扱うため)
	ImGui::SeparatorText("Depart (Start -> Game)");
	// 開始入力から、隕石が自機を追い越すまで / 視野を埋める(=真っ黒になり次のシーンへ切り替わる・BGMが消え切る)まで
	ImGui::Text("Pass Ship: %.2f s / Fill View: %.2f s", GetMeteorPassTime(), GetFillTime());
	// よけた自機と隕石の表面の間隔。負なら隕石が自機をすり抜けて見えるので赤で出す
	const float clearance = GetMeteorClearance();
	if (clearance < 0.0f) {
		ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "Clearance: %.2f m (meteor passes through ship)", clearance);
	} else {
		ImGui::Text("Clearance: %.2f m", clearance);
	}
	// 速さ・半径・判定の倍率が0だと時刻の計算が0除算になるので、手入力でも下限を割らないようにする
	ImGui::DragFloat("Meteor Speed", &departParams_.meteorSpeed, 0.5f, 5.0f, 200.0f, "%.1f m/s",
		ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Meteor Radius", &departParams_.meteorRadius, 0.05f, 0.5f, 10.0f, "%.2f m",
		ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Silhouette Ratio", &departParams_.silhouetteRatio, 0.01f, 0.3f, 1.0f, "%.2f",
		ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat("Dodge Lead##Depart", &departParams_.dodgeLeadTime, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Dodge Time##Depart", &departParams_.dodgeDuration, 0.01f, 0.0f, 3.0f, "%.2f s");
	ImGui::DragFloat("Dark Time", &departParams_.darkDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::DragFloat("Shake Time", &departParams_.shakeDuration, 0.01f, 0.0f, 2.0f, "%.2f s");
	ImGui::DragFloat("Shake Power", &departParams_.shakeStrength, 0.01f, 0.0f, 2.0f, "%.2f m");
	ImGui::DragFloat("Trail Rate", &departParams_.trailRate, 1.0f, 0.0f, 2000.0f, "%.0f /s");
	ImGui::DragFloat("Trail Spread", &departParams_.trailSpreadSpeed, 0.05f, 0.0f, 30.0f, "%.2f m/s");
	// 尾の粒の大きさ・寿命・色。粒はこの設定で出すので、"Particle"ウィンドウの既定configではなくここで変える
	if (ImGui::TreeNode("Trail Particle")) {
		ParticleManager::DrawConfigImGui(trailConfig_);
		ImGui::TreePop();
	}
	ImGui::End();
#endif
}

void TitleScene::ChangePhase(Phase phase) {
	phase_ = phase;
	phaseTimer_ = 0.0f;
	// 揺れは出発の演出の岩が出すものなので、フェーズをまたいで残さない(プレビュー後のReadyで揺れ続けないように)
	shakeTimer_ = 0.0f;
	// Readyへの入り方は「演出を見終えた」「スキップした」の2通りあるが、
	// どちらも作品名を出し切ったことになるので、記録はここ1箇所で行う
	if (phase == Phase::Ready) {
		titleShown_ = true;
	}
}

bool TitleScene::IsStartPressed() const {
	Input* input = Input::GetInstance();
	return input->IsTriggerKey(DIK_RETURN) || input->IsTriggerKey(DIK_SPACE) ||
		input->IsTriggerPad(XINPUT_GAMEPAD_A) || input->IsTriggerPad(XINPUT_GAMEPAD_START);
}

TitleScene::~TitleScene() = default;
