#include "scene/CG4DemoScene.h"

#include "io/Input.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "3d/DebugCamera.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "debug/DebugRenderer.h"
#include "effect/EffectManager.h"
#include "effect/GPUParticleEmitter.h"
#include "effect/GPUParticleManager.h"
#include "effect/ParticleEmitter.h"
#include "effect/ParticleManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// スキニングキャラクタ。walk / sneakWalk の2つのアニメーションを内包している
	const std::string kHumanModel = "human/human_re.gltf";
	// 手に持たせる武器
	const std::string kWeaponModel = "hammer/hammer.obj";
	// 地面
	const std::string kGroundModel = "ground.obj";
	// パーティクル用テクスチャ
	const std::string kParticleTexture = "resources/circle.png";

	// mixamoリグのJoint名。他のリグに差し替えるならここを直す
	const std::string kRightHandJoint = "mixamorig:RightHand";
	const std::string kLeftHandJoint = "mixamorig:LeftHand";

	const std::string kAnimationWalk = "walk";
	const std::string kAnimationSneakWalk = "sneakWalk";

	// 武器の倍率
	constexpr float kWeaponScale = 0.22f;

	// 武器の装着位置と手から出すパーティクルの発生位置に共通で使う
	const Vector3 kHandGripOffset = { 0.0f, 0.07f, 0.02f };

	constexpr float kGroundHalfSize = 25.0f;

	// キャラクタが動き回れる範囲。地面の端より内側に収める
	constexpr float kFieldLimit = kGroundHalfSize - 3.0f;
}

CG4DemoScene::CG4DemoScene() = default;
CG4DemoScene::~CG4DemoScene() = default;

void CG4DemoScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(camera_.get());

	TextureManager::GetInstance()->LoadTexture(kParticleTexture);

	ModelManager::GetInstance()->LoadModel(kHumanModel);
	ModelManager::GetInstance()->LoadModel(kWeaponModel);
	ModelManager::GetInstance()->LoadModel(kGroundModel);

	// スキニングキャラクタ本体
	human_ = std::make_unique<Object3d>();
	human_->Initialize(Object3dCommon::GetInstance());
	human_->SetModel(kHumanModel);
	human_->SetCamera(camera_.get());
	human_->SetTranslate(characterPosition_);

	currentAnimationName_ = kAnimationSneakWalk;
	human_->PlayAnimation(currentAnimationName_, true, 0.0f);

	// 武器(右手のJointへ親子付け)
	// 親子付けしておくとアニメーションに自動追従する。武器側は毎フレーム
	hammer_ = std::make_unique<Object3d>();
	hammer_->Initialize(Object3dCommon::GetInstance());
	hammer_->SetModel(kWeaponModel);
	hammer_->SetCamera(camera_.get());

	weaponJointName_ = kRightHandJoint;
	weaponOffset_.scale = { kWeaponScale, kWeaponScale, kWeaponScale };
	weaponOffset_.rotate = { 0.0f, 0.0f, std::numbers::pi_v<float> / 2.0f };
	weaponOffset_.translate = kHandGripOffset;
	hammer_->SetJointParent(human_.get(), weaponJointName_);

	// 地面
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(Object3dCommon::GetInstance());
	ground_->SetModel(kGroundModel);
	ground_->SetCamera(camera_.get());
	ground_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	ground_->SetRotate({ -std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
	ground_->SetEnableLighting(false); // 平面なので陰影は付かない
	ground_->SetScale({ kGroundHalfSize, kGroundHalfSize, 1.0f });

	// 手から出すパーティクル
	// 左手:常時放出、GPUParticle
	GPUParticleConfig auraConfig;
	auraConfig.minScale = { 0.08f, 0.08f, 0.08f };
	auraConfig.maxScale = { 0.18f, 0.18f, 0.18f };
	auraConfig.minVelocity = { -0.35f, 0.15f, -0.35f };
	auraConfig.maxVelocity = { 0.35f, 0.8f, 0.35f };
	auraConfig.acceleration = { 0.0f, 0.3f, 0.0f }; // ゆっくり上へ立ち上る
	auraConfig.lifeTimeMin = 0.25f;
	auraConfig.lifeTimeMax = 0.55f;
	auraConfig.startColor = { 0.45f, 0.85f, 1.0f, 1.0f };
	auraConfig.endColor = { 0.1f, 0.25f, 0.9f, 0.0f };
	auraConfig.endScaleRatio = 0.15f;
	handAuraEmitter_ = std::make_unique<GPUParticleEmitter>("handAura", kParticleTexture, auraConfig);
	handAuraEmitter_->SetRadius(0.07f); // 手のひらに収まる大きさ
	handAuraEmitter_->SetEmitCount(16);
	handAuraEmitter_->SetFrequency(0.02f);

	// 右手:単発、CPUParticle
	ParticleConfig burstConfig;
	burstConfig.minScale = { 0.1f, 0.1f, 0.1f };
	burstConfig.maxScale = { 0.25f, 0.25f, 0.25f };
	burstConfig.minVelocity = { -2.0f, 0.5f, -2.0f };
	burstConfig.maxVelocity = { 2.0f, 3.5f, 2.0f };
	burstConfig.acceleration = { 0.0f, -6.0f, 0.0f };
	burstConfig.lifeTimeMin = 0.4f;
	burstConfig.lifeTimeMax = 0.9f;
	burstConfig.startColor = { 1.0f, 0.85f, 0.35f, 1.0f };
	burstConfig.endColor = { 1.0f, 0.25f, 0.0f, 0.0f };
	burstConfig.endScaleRatio = 0.2f;
	handBurstEmitter_ = std::make_unique<ParticleEmitter>("handBurst", kParticleTexture, burstConfig, BlendMode::Add);
	handBurstEmitter_->SetActive(false);

	auraJointName_ = kLeftHandJoint;
	burstJointName_ = kRightHandJoint;
	handParticleOffset_ = kHandGripOffset;

	ParticleManager::GetInstance()->SetCamera(camera_.get());
	GPUParticleManager::GetInstance()->SetCamera(camera_.get());

	// 初期カメラ位置をキャラクタの後方へ置く
	UpdateFollowCamera();
	camera_->Update();
	camera_->TransferToGPU();
}

void CG4DemoScene::Finalize() {
}

Vector3 CG4DemoScene::ReadMoveInput() const {
	Input* input = Input::GetInstance();

	Vector3 move{ 0.0f, 0.0f, 0.0f };

	// パッドが繋がっていればスティックを優先する(資料の「パッドで動かせる」に対応)
	if (input->IsControllerConnected()) {
		move.x = input->GetLeftStickX();
		move.z = input->GetLeftStickY();
	}

	// パッドが無い環境でも確認できるようにキーボードも受け付ける
	if (input->IsPushKey(DIK_A)) { move.x -= 1.0f; }
	if (input->IsPushKey(DIK_D)) { move.x += 1.0f; }
	if (input->IsPushKey(DIK_S)) { move.z -= 1.0f; }
	if (input->IsPushKey(DIK_W)) { move.z += 1.0f; }

	// 斜め入力が速くならないよう、長さが1を超えたときだけ正規化する
	const float length = move.Length();
	if (length > 1.0f) {
		move /= length;
	}
	return move;
}

void CG4DemoScene::UpdateAnimationState(float moveAmount) {
	if (!autoAnimationSwitch_) {
		return;
	}

	// 入力量がしきい値を超えたら walk、それ以下なら sneakWalk。
	// PlayAnimationにblendDurationを渡すと、AnimationPlayerが遷移前後の2つの
	// アニメーションを同時に再生して補間するので、切り替わりが滑らかになる
	const std::string& nextAnimation =
		(moveAmount >= walkThreshold_) ? kAnimationWalk : kAnimationSneakWalk;

	if (nextAnimation == currentAnimationName_) {
		return;
	}
	currentAnimationName_ = nextAnimation;
	human_->PlayAnimation(currentAnimationName_, true, blendDuration_);
}

void CG4DemoScene::UpdateFollowCamera() {
	if (!followCameraActive_) {
		return;
	}

	// キャラクタの向きにcameraOrbitYaw_を足した方向から見る。
	// 0なら背中側、πなら正面。これでキャラクタを動かしたまま好きな角度で観察できる
	const float viewAngle = characterAngle_ + cameraOrbitYaw_;

	// 行ベクトル規約なので forward = (sin(yaw), 0, cos(yaw))
	const Vector3 forward{ std::sin(viewAngle), 0.0f, std::cos(viewAngle) };
	const Vector3 target = characterPosition_ + Vector3{ 0.0f, cameraHeight_, 0.0f };

	camera_->SetTranslate(target - forward * cameraDistance_ + Vector3{ 0.0f, cameraDistance_ * cameraPitch_, 0.0f });
	camera_->SetRotate({ cameraPitch_, viewAngle, 0.0f });
}

Camera* CG4DemoScene::GetActiveCamera() const {
	return debugCamera_->IsActive() ? debugCamera_->GetCamera() : camera_.get();
}

void CG4DemoScene::Update(float deltaTime) {

	Input* input = Input::GetInstance();

	// F1でデバッグカメラON/OFF。ON中は追従カメラがカメラのTransformを上書きしない
	if (input->IsTriggerKey(DIK_F1)) {
		debugCamera_->SetActive(!debugCamera_->IsActive());
	}
	debugCamera_->Update(deltaTime);

	// --- キャラクタの移動 ------------------------------------------------
	const Vector3 moveInput = ReadMoveInput();
	currentMoveAmount_ = moveInput.Length();

	if (currentMoveAmount_ > 0.0f) {
		// 位置を固定していても向きは変えたいので、移動量だけを止める
		// (歩行アニメーションとその場での向き変えは残る)
		if (!lockPosition_) {
			// 速度は「毎秒量×dt」で積む
			characterPosition_ += moveInput * (moveSpeed_ * deltaTime);
			characterPosition_.x = std::clamp(characterPosition_.x, -kFieldLimit, kFieldLimit);
			characterPosition_.z = std::clamp(characterPosition_.z, -kFieldLimit, kFieldLimit);
		}

		// 進行方向へ向き直る。角度差を±πに畳んでから補間しないと、
		// -π と +π をまたぐときに一周回ってしまう
		const float targetAngle = std::atan2(moveInput.x, moveInput.z);
		float difference = targetAngle - characterAngle_;
		constexpr float twoPi = std::numbers::pi_v<float> * 2.0f;
		difference = std::fmod(difference + std::numbers::pi_v<float>, twoPi);
		if (difference < 0.0f) { difference += twoPi; }
		difference -= std::numbers::pi_v<float>;

		// 指数補間: 1フレームで詰める割合をdt基準にして、フレームレートに依存させない
		characterAngle_ += difference * std::min(turnLerpRate_ * deltaTime, 1.0f);
	}

	human_->SetTranslate(characterPosition_);
	human_->SetRotate({ 0.0f, characterAngle_, 0.0f });

	UpdateAnimationState(currentMoveAmount_);

	// --- カメラの周回 ------------------------------------------------------
	// 右スティック / Q・Eでキャラクタの周りを回る。移動しながらでも正面や横から観察できる
	float orbitInput = 0.0f;
	if (input->IsControllerConnected()) {
		orbitInput += input->GetRightStickX();
	}
	if (input->IsPushKey(DIK_Q)) { orbitInput -= 1.0f; }
	if (input->IsPushKey(DIK_E)) { orbitInput += 1.0f; }
	cameraOrbitYaw_ += orbitInput * cameraOrbitSpeed_ * deltaTime;

	UpdateFollowCamera();

	Camera* activeCamera = GetActiveCamera();
	human_->SetCamera(activeCamera);
	hammer_->SetCamera(activeCamera);
	ground_->SetCamera(activeCamera);
	ParticleManager::GetInstance()->SetCamera(activeCamera);
	GPUParticleManager::GetInstance()->SetCamera(activeCamera);

	camera_->Update();
	camera_->TransferToGPU();

	// --- オブジェクト更新 --------------------------------------------------
	// human_→hammer_の順に更新すること。hammer_はUpdateの中でhuman_の
	// 「今フレームのJointのワールド行列」を読むため、逆順だと1フレーム遅れる
	human_->Update(deltaTime);

	hammer_->SetJointParent(equipWeapon_ ? human_.get() : nullptr, weaponJointName_);
	hammer_->SetScale(weaponOffset_.scale);
	hammer_->SetRotate(weaponOffset_.rotate);
	hammer_->SetTranslate(weaponOffset_.translate);
	hammer_->Update(deltaTime);

	ground_->Update(deltaTime);

	// --- 手からパーティクル ------------------------------------------------
	// Skeletonから手のJointのワールド座標を取り出し、エミッタをそこへ置く。
	// これでキャラクタ中心ではなく「手から」出ているように見える
	if (std::optional<Vector3> auraPosition =
		human_->GetJointAttachPosition(auraJointName_, handParticleOffset_)) {
		handAuraEmitter_->SetPosition(*auraPosition);
		handAuraEmitter_->SetActive(handAuraActive_);
	} else {
		handAuraEmitter_->SetActive(false);
	}

	// Aボタン / スペースで右手から単発バースト
	const bool burstTriggered =
		input->IsTriggerPad(XINPUT_GAMEPAD_A) || input->IsTriggerKey(DIK_SPACE);
	if (burstTriggered) {
		if (std::optional<Vector3> burstPosition =
			human_->GetJointAttachPosition(burstJointName_, handParticleOffset_)) {
			handBurstEmitter_->EmitAt(*burstPosition, burstCount_);
		}
	}

	// --- 骨のデバッグ表示 --------------------------------------------------
	// DebugRendererは毎フレーム積み直す方式なのでUpdateで積む
	if (drawSkeleton_) {
		SkeletonDebug::Draw(human_->GetSkeleton(), human_->GetWorldMatrix(), skeletonOptions_);
	}

	DebugRenderer::GetInstance()->AddGrid({ 0.0f, 0.0f, 0.0f }, 10.0f, 20, { 1.0f, 1.0f, 1.0f, 0.3f });
}

void CG4DemoScene::Draw() {
	ground_->Draw();
	// メッシュを描かなくてもUpdateでスケルトンは更新済みなので、骨の表示と武器の追従は続く
	if (!hideMesh_) {
		human_->Draw();
	}
	if (equipWeapon_) {
		hammer_->Draw();
	}

	DebugRenderer::GetInstance()->RenderAll(*GetActiveCamera());
}

void CG4DemoScene::DrawImGui() {
#ifdef USE_IMGUI

	// 関節名はSceneイメージ上へ重ねて描くので、ウィンドウの外で呼ぶ
	if (drawSkeleton_) {
		SkeletonDebug::DrawNames(human_->GetSkeleton(), human_->GetWorldMatrix(),
			*GetActiveCamera(), skeletonOptions_);
	}

	ImGui::Begin("CG4 Demo");

	// ImGuiのフォントは既定のASCIIのみ(日本語グリフを読み込んでいない)ため表示は英語で統一する
	ImGui::TextWrapped("LStick / WASD: Move   RStick / Q,E: Orbit camera   "
		"Pad A / Space: Burst from right hand   F1: Debug Camera");

	if (ImGui::CollapsingHeader("Skinning / Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Current: %s", currentAnimationName_.c_str());
		ImGui::Text("Move Amount: %.2f", currentMoveAmount_);
		ImGui::ProgressBar(human_->GetAnimationProgress(), ImVec2(-1.0f, 0.0f));
		ImGui::Text("Frame: %d / %d",
			human_->GetAnimationCurrentFrame(), human_->GetAnimationTotalFrames());

		ImGui::Checkbox("Auto Switch", &autoAnimationSwitch_);
		ImGui::DragFloat("Blend Duration", &blendDuration_, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Walk Threshold", &walkThreshold_, 0.01f, 0.0f, 1.0f);

		// 補間の効きを確かめるための手動切り替え
		ImGui::BeginDisabled(autoAnimationSwitch_);
		if (ImGui::Button("Play walk")) {
			currentAnimationName_ = kAnimationWalk;
			human_->PlayAnimation(currentAnimationName_, true, blendDuration_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Play sneakWalk")) {
			currentAnimationName_ = kAnimationSneakWalk;
			human_->PlayAnimation(currentAnimationName_, true, blendDuration_);
		}
		ImGui::EndDisabled();

		ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.0f, 20.0f);
	}

	if (ImGui::CollapsingHeader("Skeleton Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Draw Skeleton", &drawSkeleton_);
		ImGui::SameLine();
		// 骨は深度テストでメッシュに隠れるため、単体で見たいときはメッシュを消す
		ImGui::Checkbox("Hide Mesh", &hideMesh_);
		ImGui::BeginDisabled(!drawSkeleton_);
		SkeletonDebug::DrawImGui(human_->GetSkeleton(), skeletonOptions_);
		ImGui::EndDisabled();
		ImGui::Text("Joints: %d", static_cast<int>(human_->GetSkeleton().joints.size()));
	}

	if (ImGui::CollapsingHeader("Weapon Attach", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Equip", &equipWeapon_);
		ImGui::Text("Joint: %s", weaponJointName_.c_str());
		ImGui::DragFloat3("Offset Translate", &weaponOffset_.translate.x, 0.005f);
		ImGui::DragFloat3("Offset Rotate", &weaponOffset_.rotate.x, 0.01f);
		ImGui::DragFloat3("Offset Scale", &weaponOffset_.scale.x, 0.005f, 0.001f, 5.0f);
		if (ImGui::Button("Reset Offset")) {
			weaponOffset_.scale = { kWeaponScale, kWeaponScale, kWeaponScale };
			weaponOffset_.rotate = { 0.0f, 0.0f, std::numbers::pi_v<float> / 2.0f };
			weaponOffset_.translate = kHandGripOffset;
		}
	}

	if (ImGui::CollapsingHeader("Hand Particle", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Hand Aura (GPU)", &handAuraActive_);
		ImGui::Text("Aura Joint : %s", auraJointName_.c_str());
		ImGui::Text("Burst Joint: %s", burstJointName_.c_str());
		// HandのJointは手首なので、手のひらまでずらす量。武器のOffset Translateと同じ基準
		ImGui::DragFloat3("Grip Offset", &handParticleOffset_.x, 0.005f);
		int count = static_cast<int>(burstCount_);
		if (ImGui::DragInt("Burst Count", &count, 1.0f, 1, 500)) {
			burstCount_ = static_cast<uint32_t>(count);
		}
		if (ImGui::Button("Burst Now")) {
			if (std::optional<Vector3> burstPosition =
			human_->GetJointAttachPosition(burstJointName_, handParticleOffset_)) {
				handBurstEmitter_->EmitAt(*burstPosition, burstCount_);
			}
		}
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Follow Camera", &followCameraActive_);
		ImGui::SameLine();
		// 見た目を詰めている間はその場に留めておきたいので、移動だけ止められるようにする
		ImGui::Checkbox("Lock Position", &lockPosition_);

		// キャラクタの向きを基準にした周回角。ワンボタンで定番のアングルへ飛べるようにする
		constexpr float pi = std::numbers::pi_v<float>;
		if (ImGui::Button("Front")) { cameraOrbitYaw_ = pi; }
		ImGui::SameLine();
		if (ImGui::Button("Back")) { cameraOrbitYaw_ = 0.0f; }
		ImGui::SameLine();
		if (ImGui::Button("Left")) { cameraOrbitYaw_ = pi * 0.5f; }
		ImGui::SameLine();
		if (ImGui::Button("Right")) { cameraOrbitYaw_ = -pi * 0.5f; }

		ImGui::SliderFloat("Orbit Yaw", &cameraOrbitYaw_, -pi, pi);
		ImGui::DragFloat("Height", &cameraHeight_, 0.05f, 0.0f, 10.0f);
		ImGui::DragFloat("Distance", &cameraDistance_, 0.1f, 0.5f, 30.0f);
		ImGui::DragFloat("Pitch", &cameraPitch_, 0.01f, -1.0f, 1.0f);
	}

	ImGui::End();

	debugCamera_->DrawImGui();
#endif
}
