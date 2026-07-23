#include "scene/PostEffectDemoScene.h"

#include "io/Input.h"
#include "3d/Camera.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "3d/Object3d.h"
#include "base/DirectXCommon.h"
#include "effect/EffectManager.h"
#include "effect/DepthBasedOutline.h"
#include "effect/RadialBlur.h"
#include "effect/ParticleManager.h"
#include "effect/GPUParticleManager.h"

#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 数字キー(1〜9)と対応するエフェクト名。並びはCG5評価課題1の加点一覧に合わせている
	// (Grayscale=Monochrome、Random=Noise が本エンジンでの実装名)
	struct KeyEffect {
		BYTE key;
		const char* label;      // 画面表示用(課題での呼び名)
		const char* effectName; // EffectManager::FindEffectに渡す実装名
	};
	const KeyEffect kKeyEffects[] = {
		{ DIK_1, "1:Grayscale",             "Monochrome" },
		{ DIK_2, "2:Vignetting",            "Vignette" },
		{ DIK_3, "3:BoxFilter",             "BoxFilter" },
		{ DIK_4, "4:GaussianFilter",        "GaussianFilter" },
		{ DIK_5, "5:LuminanceBasedOutline", "LuminanceBasedOutline" },
		{ DIK_6, "6:DepthBasedOutline",     "DepthBasedOutline" },
		{ DIK_7, "7:RadialBlur",            "RadialBlur" },
		{ DIK_8, "8:Dissolve",              "Dissolve" },
		{ DIK_9, "9:Random",                "Noise" },
	};
}

PostEffectDemoScene::PostEffectDemoScene() = default;
PostEffectDemoScene::~PostEffectDemoScene() = default;

void PostEffectDemoScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	// 資料スライドと同じ「terrainを見下ろして中央に球」の構図
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 7.5f, -20.0f });

	// パーティクルManagerの更新はFrameworkが常時回すため、カメラだけは渡しておく
	ParticleManager::GetInstance()->SetCamera(camera_.get());
	GPUParticleManager::GetInstance()->SetCamera(camera_.get());

	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");

	terrain_ = std::make_unique<Object3d>();
	terrain_->Initialize(Object3dCommon::GetInstance());
	terrain_->SetModel("terrain.obj");
	terrain_->SetCamera(camera_.get());
	terrain_->SetTranslate({ 0.0f, 0.0f, 0.0f });

	// 中央のモンスターボール(sphere.objのテクスチャがmonsterBall.png)
	ball_ = std::make_unique<Object3d>();
	ball_->Initialize(Object3dCommon::GetInstance());
	ball_->SetModel("sphere.obj");
	ball_->SetCamera(camera_.get());
	ball_->SetTranslate({ 0.0f, 1.0f, 0.0f });

	// 奥にもう1つ。DepthBasedOutline(距離差)とLuminanceBasedOutline(模様)の違いを見比べる用
	ballFar_ = std::make_unique<Object3d>();
	ballFar_->Initialize(Object3dCommon::GetInstance());
	ballFar_->SetModel("sphere.obj");
	ballFar_->SetCamera(camera_.get());
	ballFar_->SetTranslate({ -6.0f, 1.5f, 12.0f });
	ballFar_->SetScale({ 1.5f, 1.5f, 1.5f });

	// DepthBasedOutlineはNDC深度→ビューZ復元にprojectionInverseが要るのでカメラを渡す
	if (auto* effect = effectManager_->FindEffect("DepthBasedOutline")) {
		static_cast<DepthBasedOutline*>(effect)->SetCamera(camera_.get());
	}
	// RadialBlurはイベント駆動(Trigger)前提で常時掛け強度の既定が0のため、
	// enabledにしても画面が変化しない。デモでは常時掛けの強度を入れておく
	if (auto* effect = effectManager_->FindEffect("RadialBlur")) {
		static_cast<RadialBlur*>(effect)->SetIntensity(0.6f);
	}
}

void PostEffectDemoScene::Finalize() {
}

void PostEffectDemoScene::Update(float deltaTime) {
	Input* input = Input::GetInstance();

	// 数字キーで排他切替: 一旦全OFFにしてから押されたものだけON
	for (const KeyEffect& keyEffect : kKeyEffects) {
		if (input->IsTriggerKey(keyEffect.key)) {
			effectManager_->ResetAll();
			if (auto* effect = effectManager_->FindEffect(keyEffect.effectName)) {
				effect->enabled = true;
				activeEffectName_ = keyEffect.label;
			}
		}
	}
	if (input->IsTriggerKey(DIK_0)) { // 0で全OFF(元画像)
		effectManager_->ResetAll();
		activeEffectName_ = "None";
	}

	camera_->Update();
	camera_->TransferToGPU();

	terrain_->Update(deltaTime);
	ball_->Update(deltaTime);
	ballFar_->Update(deltaTime);
}

void PostEffectDemoScene::Draw() {
	terrain_->Draw();
	ball_->Draw();
	ballFar_->Draw();
}

void PostEffectDemoScene::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("PostEffect Demo");
	ImGui::Text("Active: %s", activeEffectName_.c_str());
	ImGui::Separator();
	for (const KeyEffect& keyEffect : kKeyEffects) {
		ImGui::Text("%s", keyEffect.label);
	}
	ImGui::Text("0:OFF (original)");
	ImGui::End();
#endif
}
