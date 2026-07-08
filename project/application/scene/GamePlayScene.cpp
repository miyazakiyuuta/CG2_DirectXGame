#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "2d/TextureManager.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/ParticleEmitter.h"
#include "effect/GPUParticleEmitter.h"
#include "3d/Object3d.h"
#include "3d/Skybox.h"
#include "3d/SkyCylinder.h"
#include "debug/DebugRenderer.h"
#include "effect/EffectManager.h"
#include "effect/DepthBasedOutline.h"

#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#include "debug/EditorPanels.h"
#include "debug/TransformGizmo.h"
#endif

namespace {
	// シーン配置の保存先(作業ディレクトリ=プロジェクト直下からの相対パス)
	const std::string kScenePath = "resources/scenes/GamePlayScene.json";
}

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,7.5f,-20.0f });

	// パーティクルにこのシーンのアクティブカメラを渡す(初期化・更新・描画はFrameworkが行う)
	ParticleManager::GetInstance()->SetCamera(camera_.get());

	// CPUパーティクルのサンプル: 火花の噴水
	ParticleConfig sparkConfig;
	sparkConfig.minScale = { 0.3f, 0.3f, 0.3f };
	sparkConfig.maxScale = { 0.6f, 0.6f, 0.6f };
	sparkConfig.minVelocity = { -1.5f, 4.0f, -1.5f };
	sparkConfig.maxVelocity = { 1.5f, 7.0f, 1.5f };
	sparkConfig.acceleration = { 0.0f, -9.8f, 0.0f }; // 重力
	sparkConfig.lifeTimeMin = 0.6f;
	sparkConfig.lifeTimeMax = 1.4f;
	sparkConfig.startColor = { 1.0f, 0.8f, 0.3f, 1.0f };
	sparkConfig.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };
	sparkConfig.endScaleRatio = 0.2f;
	sparkEmitter_ = std::make_unique<ParticleEmitter>("spark", "resources/circle.png", sparkConfig, BlendMode::Add);
	sparkEmitter_->SetPosition({ 3.0f, 0.5f, 0.0f });
	sparkEmitter_->SetCount(6);
	sparkEmitter_->SetEmitInterval(0.1f);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// TextureManager からテクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");
	TextureManager::GetInstance()->LoadTexture("resources/circle.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("human/sneakWalk.gltf");
	ModelManager::GetInstance()->LoadModel("human/human_re.gltf");
	ModelManager::GetInstance()->LoadModel("Frog/Frog.gltf");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	//object3d_->SetModel("human/human_re.gltf");
	object3d_->SetModel("sphere.obj");
	object3d_->SetCamera(camera_.get());
	object3d_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	//object3d_->SetColor({ 0.5f,0.5f,0.5f,1.0f });
	//object3d_->SetUseEnvironmentMap(true); // 環境マップ

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

	skyCylinder_ = std::make_unique<SkyCylinder>();
	skyCylinder_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), "resources/uvChecker.png");
	skyCylinder_->SetCamera(camera_.get());
	skyCylinder_->GetTransform().scale = { 50.0f, 20.0f, 50.0f };
	skyCylinder_->GetTransform().translate = { 0.0f,  -5.0f,  0.0f };

	// Hierarchy/Inspector/ギズモ/保存読込が共有するオブジェクト一覧。
	// オブジェクトを追加したらここに1行足すだけで全機能に反映される。
	// SkyCylinderは全天を覆うため、Cameraは実体が見えないためクリック選択の対象外
	editorObjects_.clear();
	editorObjects_.push_back({ "Sphere", &object3d_->GetTransform(), true });
	editorObjects_.push_back({ "SkyCylinder", &skyCylinder_->GetTransform(), false });
	editorObjects_.push_back({ "Camera", &camera_->GetTransform(), false });
	editorObjects_.back().scaleEditable = false; // カメラのscaleはビュー行列を歪ませるため編集させない
	selectedIndex_ = -1;

#ifdef USE_IMGUI
	// 型別のInspector追加UI(ImGui呼び出しを含むためDebug構成のみ)
	editorObjects_.back().drawInspector = [this]() {
		float fovY = camera_->GetFovY();
		if (ImGui::DragFloat("FovY", &fovY, 0.01f)) {
			camera_->SetFovY(fovY);
		}
	};
#endif

	// 保存済みのシーン配置があれば復元(無ければ上の初期値のまま)
	SceneSerializer::Load(kScenePath, BuildSerializeEntries());

	effectManager_->FindEffect("Monochrome")->enabled = false;
	effectManager_->FindEffect("RadialBlur")->enabled = false;
	//effectManager_->FindEffect("DepthBasedOutline")->enabled = true;
	if (auto* e = effectManager_->FindEffect("DepthBasedOutline")) {
		auto* outline = static_cast<DepthBasedOutline*>(e);
		outline->SetCamera(camera_.get()); // このシーンのアクティブカメラ
		outline->enabled = true;
	}

}

void GamePlayScene::Finalize() {
}

void GamePlayScene::Update(float deltaTime) {

	skyCylinder_->Update();

	camera_->Update();
	camera_->TransferToGPU();
	if (Input::GetInstance()->IsTriggerKey(DIK_0)) {
		object3d_->StopAnimation();
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_9)) {
		object3d_->StopAnimation(0.5f);
	}
	if (Input::GetInstance()->IsPushKey(DIK_1)) {
		object3d_->PlayAnimation("walk", true, 0.1f);
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_2)) {
		object3d_->PlayAnimation("sneakWalk", true, 0.1f);
	}
	// 単発バーストのサンプル(ヒットエフェクト想定)
	if (Input::GetInstance()->IsTriggerKey(DIK_3)) {
		sparkEmitter_->EmitAt(object3d_->GetTranslate(), 30);
	}

	object3d_->Update(deltaTime);

	DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 10.0f, 20, { 1.0f,1.0f,1.0f,0.5f });

}

void GamePlayScene::Draw() {
	//skybox_->Draw(*camera_);
	skyCylinder_->Draw();

	object3d_->Draw();

	DebugRenderer::GetInstance()->RenderAll(*camera_);
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	// Hierarchy: オブジェクト一覧+選択、シーンファイル操作
	ImGui::Begin("Hierarchy");
	EditorPanels::DrawHierarchy(editorObjects_, selectedIndex_);
	ImGui::SeparatorText("Scene File");
	if (ImGui::Button("Save")) {
		SceneSerializer::Save(kScenePath, BuildSerializeEntries());
	}
	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		SceneSerializer::Load(kScenePath, BuildSerializeEntries());
	}
	ImGui::End();

	// Inspector: ギズモ操作モード+選択中オブジェクトの編集
	ImGui::Begin("Inspector");
	TransformGizmo::DrawOperationSelector();
	EditorPanels::DrawInspector(editorObjects_, selectedIndex_);
	ImGui::End();

	// Sceneビューのクリックで選択を更新し、選択中の対象にギズモを表示
	TransformGizmo::PickBySceneClick(*camera_, editorObjects_, selectedIndex_);
	if (selectedIndex_ >= 0) {
		TransformGizmo::Manipulate(*camera_, editorObjects_[selectedIndex_]);
	}

#endif
}

std::vector<SceneSerializer::Entry> GamePlayScene::BuildSerializeEntries() const {
	// エディタ一覧がそのまま保存対象(一覧が唯一の定義箇所になる)
	std::vector<SceneSerializer::Entry> entries;
	entries.reserve(editorObjects_.size());
	for (const EditorObject& object : editorObjects_) {
		entries.push_back({ object.name, object.transform });
	}
	return entries;
}

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;