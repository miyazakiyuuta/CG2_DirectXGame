#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "base/SrvManager.h"
#include "2d/TextureManager.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/ParticleEmitter.h"
#include "effect/GPUParticleManager.h"
#include "effect/GPUParticleEmitter.h"
#include "3d/Object3d.h"
#include "3d/Skybox.h"
#include "stage/Stage.h"
#include "3d/SkyCylinder.h"
#include "debug/DebugRenderer.h"
#include "effect/EffectManager.h"
#include "effect/DepthBasedOutline.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#include <filesystem>
#include "debug/EditorPanels.h"
#include "debug/TransformGizmo.h"
#include "math/Matrix4x4.h"
#include "stage/StageSerializer.h"
#endif

namespace {
	// シーン配置の保存先(作業ディレクトリ=プロジェクト直下からの相対パス)
	const std::string kScenePath = "resources/scenes/GamePlayScene.json";
	// ステージデータの置き場所(「エディタで作ってゲームが読む」パイプラインの受け渡しファイル)
	const std::string kStagePath = "resources/scenes/stage.json";
}

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,7.5f,-20.0f });

	// パーティクルにこのシーンのアクティブカメラを渡す(初期化・更新・描画はFrameworkが行う)
	ParticleManager::GetInstance()->SetCamera(camera_.get());
	GPUParticleManager::GetInstance()->SetCamera(camera_.get());

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

	// GPUパーティクルのサンプル: 空間を漂う塵(大量・常時放出はGPU側の担当)
	// 定常時の生存数 ≈ emitCount × 平均寿命 ÷ frequency = 1000 × 6 ÷ 0.1 = 約6万粒
	GPUParticleConfig dustConfig;
	dustConfig.minScale = { 0.05f, 0.05f, 0.05f };
	dustConfig.maxScale = { 0.15f, 0.15f, 0.15f };
	dustConfig.minVelocity = { -0.3f, 0.1f, -0.3f };
	dustConfig.maxVelocity = { 0.3f, 0.8f, 0.3f };
	dustConfig.lifeTimeMin = 4.0f;
	dustConfig.lifeTimeMax = 8.0f;
	dustConfig.startColor = { 0.6f, 0.8f, 1.0f, 0.8f };
	dustConfig.endColor = { 0.2f, 0.4f, 1.0f, 0.0f };
	dustConfig.endScaleRatio = 0.5f;
	gpuParticleEmitter_ = std::make_unique<GPUParticleEmitter>("gpuDust", "resources/circle.png", dustConfig);
	gpuParticleEmitter_->SetPosition({ 0.0f, 3.0f, 0.0f });
	gpuParticleEmitter_->SetRadius(15.0f);
	gpuParticleEmitter_->SetEmitCount(1000);
	gpuParticleEmitter_->SetFrequency(0.1f);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(camera_.get());

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

	// ステージ配置をstage.jsonから構築(必要なモデルはStage側がロードする)
	stage_ = std::make_unique<Stage>();
	stage_->SetCamera(camera_.get());
	stage_->LoadFromFile(kStagePath);

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

#ifdef USE_IMGUI
	// エディタのモデル選択Comboに出す一覧(実行中にファイルを足したらRescanボタンで取り直す)
	RescanModelFiles();
#endif

	// Hierarchy/Inspector/ギズモ/保存読込が共有するオブジェクト一覧を構築
	RebuildEditorObjects();

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

	// F1でデバッグカメラON/OFF(ImGuiのDebug Cameraウィンドウでも切替可)
	if (Input::GetInstance()->IsTriggerKey(DIK_F1)) {
		debugCamera_->SetActive(!debugCamera_->IsActive());
	}
#ifdef USE_IMGUI
	// Fキーで選択中オブジェクトへフォーカス(Sceneビュー上でのみ反応)
	if (debugCamera_->IsActive() && selectedIndex_ >= 0 &&
		Input::GetInstance()->IsSceneViewHovered() && Input::GetInstance()->IsTriggerKey(DIK_F)) {
		const Transform* target = editorObjects_[selectedIndex_].transform;
		float radius = (std::max)({ target->scale.x, target->scale.y, target->scale.z });
		debugCamera_->FocusOn(target->translate, radius);
	}
#endif
	debugCamera_->Update(deltaTime);

	// デバッグカメラON中は描画に使うカメラを差し替える(ゲームカメラのTransformは汚さない)
	Camera* activeCamera = GetActiveCamera();
	ParticleManager::GetInstance()->SetCamera(activeCamera);
	GPUParticleManager::GetInstance()->SetCamera(activeCamera);
	object3d_->SetCamera(activeCamera);
	stage_->SetCamera(activeCamera);
	skyCylinder_->SetCamera(activeCamera);
	if (auto* effect = effectManager_->FindEffect("DepthBasedOutline")) {
		static_cast<DepthBasedOutline*>(effect)->SetCamera(activeCamera);
	}

	skyCylinder_->Update();

	// レールカメラ: レール上を等速で進み、進行方向(接線)を向く。
	// camera_->Update()より前にTransformを書き、同フレームのview行列へ反映させる
	if (railCameraActive_) {
		const CatmullRomSpline& rail = stage_->GetRail();
		if (rail.GetTotalLength() > 0.0f) {
			railDistance_ += railSpeed_ * deltaTime;
			// 終端はループ(デバッグで周回し続けられるように)
			railDistance_ = std::fmod(railDistance_, rail.GetTotalLength());

			camera_->SetTranslate(rail.GetPositionByDistance(railDistance_));

			// 接線→オイラー角。回転規約(行ベクトル×Rx→Ry→Rz)では
			// forward = (cos(pitch)·sin(yaw), -sin(pitch), cos(pitch)·cos(yaw)) なので逆算する
			Vector3 tangent = rail.GetTangentByDistance(railDistance_);
			float yaw = std::atan2(tangent.x, tangent.z);
			float pitch = std::atan2(-tangent.y, std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z));
			camera_->SetRotate({ pitch, yaw, 0.0f }); // バンク(roll)はしない
		}
	}

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
	stage_->Update(deltaTime);

	DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 10.0f, 20, { 1.0f,1.0f,1.0f,0.5f });

	// レール曲線の可視化(stage.json手編集→Reloadの確認用)。曲線=赤の折れ線、制御点=黄の球
	const CatmullRomSpline& rail = stage_->GetRail();
	const std::vector<Vector3>& railPoints = rail.GetControlPoints();
	if (railPoints.size() >= 2) {
		// 1区間16分割でサンプリングして折れ線として描く
		const int division = static_cast<int>(railPoints.size() - 1) * 16;
		Vector3 prevPos = rail.GetPosition(0.0f);
		for (int i = 1; i <= division; ++i) {
			float t = static_cast<float>(i) / static_cast<float>(division);
			Vector3 pos = rail.GetPosition(t);
			DebugRenderer::GetInstance()->AddLine(prevPos, pos, { 1.0f,0.2f,0.2f,1.0f });
			prevPos = pos;
		}
	}
	for (const Vector3& point : railPoints) {
		DebugRenderer::GetInstance()->AddSphere(point, 0.5f, { 1.0f,1.0f,0.2f,1.0f });
	}
}

void GamePlayScene::Draw() {
	//skybox_->Draw(*camera_);
	skyCylinder_->Draw();

	stage_->Draw();
	object3d_->Draw();

	DebugRenderer::GetInstance()->RenderAll(*GetActiveCamera());
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	// Hierarchy: オブジェクト一覧+選択、シーンファイル操作
	ImGui::Begin("Hierarchy");
	EditorPanels::DrawHierarchy(editorObjects_, selectedIndex_);

	// ステージオブジェクトの追加/複製/削除(stage.jsonの管轄分のみ。C++直書きの固定オブジェクトは対象外)
	ImGui::SeparatorText("Stage Objects");
	const bool stageObjectSelected = selectedIndex_ >= static_cast<int>(fixedEditorObjectCount_);
	const size_t stageIndex = stageObjectSelected ? selectedIndex_ - fixedEditorObjectCount_ : 0;

	// Addで使うモデルの選択(Rescanボタンが横に並ぶため、Comboは残り幅の6割に抑える)
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
	const char* addModelPreview =
		modelFiles_.empty() ? "(no models)" : modelFiles_[addModelIndex_].c_str();
	if (ImGui::BeginCombo("##addModel", addModelPreview)) {
		for (int i = 0; i < static_cast<int>(modelFiles_.size()); ++i) {
			if (ImGui::Selectable(modelFiles_[i].c_str(), addModelIndex_ == i)) {
				addModelIndex_ = i;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button("Rescan")) {
		RescanModelFiles();
	}

	ImGui::BeginDisabled(modelFiles_.empty());
	if (ImGui::Button("Add")) {
		StageData::ObjectData objectData;
		objectData.model = modelFiles_[addModelIndex_];
		// 名前はモデルファイル名から拡張子を除いたものを既定にする("fence/fence.obj" → "fence")
		objectData.name = std::filesystem::path(objectData.model).stem().string();
		// 今見ている場所に出るよう、アクティブカメラの前方に生成する
		const Transform& cameraTransform = GetActiveCamera()->GetTransform();
		Vector3 forward = Matrix4x4::Rotate(cameraTransform.rotate).Transform({ 0.0f, 0.0f, 1.0f });
		objectData.transform.translate = cameraTransform.translate + forward * 10.0f;

		size_t newIndex = stage_->AddObject(std::move(objectData));
		RebuildEditorObjects();
		selectedIndex_ = static_cast<int>(fixedEditorObjectCount_ + newIndex);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!stageObjectSelected);
	if (ImGui::Button("Duplicate")) {
		size_t newIndex = stage_->DuplicateObject(stageIndex);
		RebuildEditorObjects();
		selectedIndex_ = static_cast<int>(fixedEditorObjectCount_ + newIndex);
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete")) {
		stage_->RemoveObject(stageIndex);
		RebuildEditorObjects(); // 選択解除もここで行われる
	}
	ImGui::EndDisabled();

	// ステージ配置のファイル操作。Reloadはstage.jsonの内容で上書き(未保存の編集は破棄される)
	ImGui::SeparatorText("Stage File");
	if (ImGui::Button("Save##stage")) {
		StageSerializer::Save(kStagePath, stage_->GetData());
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload##stage")) {
		// 失敗時(JSON破損等)はLoadFromFileが現状維持するので、一覧の作り直しも不要
		if (stage_->LoadFromFile(kStagePath)) {
			RebuildEditorObjects(); // 構造変更でTransformポインタが無効になるため必須(選択解除も行われる)
		}
	}

	// レールカメラの操作。毎フレームゲームカメラを上書きするため、
	// 配置編集(ギズモ/Inspector)したいときはここでOFFにする
	ImGui::SeparatorText("Rail Camera");
	ImGui::Checkbox("Active##railCamera", &railCameraActive_);
	ImGui::SameLine();
	if (ImGui::Button("Reset##railCamera")) {
		railDistance_ = 0.0f; // 周回を待たずに先頭から確認し直す用
	}
	ImGui::DragFloat("Speed", &railSpeed_, 0.1f, 0.0f, 100.0f, "%.1f m/s");
	ImGui::Text("Distance: %.1f / %.1f m", railDistance_, stage_->GetRail().GetTotalLength());

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

	// デバッグカメラの切替・速度・感度
	debugCamera_->DrawImGui();

	// Sceneビューのクリックで選択を更新し、選択中の対象にギズモを表示
	// (デバッグカメラON中はその視点で描画されているため、ピック・ギズモも同じカメラで行う)
	Camera* activeCamera = GetActiveCamera();
	TransformGizmo::PickBySceneClick(*activeCamera, editorObjects_, selectedIndex_);
	if (selectedIndex_ >= 0) {
		TransformGizmo::Manipulate(*activeCamera, editorObjects_[selectedIndex_]);
	}

#endif
}

std::vector<SceneSerializer::Entry> GamePlayScene::BuildSerializeEntries() const {
	// 固定オブジェクトだけが保存対象。ステージ分はstage.jsonの管轄なのでここには含めない
	std::vector<SceneSerializer::Entry> entries;
	entries.reserve(fixedEditorObjectCount_);
	for (size_t i = 0; i < fixedEditorObjectCount_; ++i) {
		entries.push_back({ editorObjects_[i].name, editorObjects_[i].transform });
	}
	return entries;
}

void GamePlayScene::RebuildEditorObjects() {
	// C++直書きの固定オブジェクト。追加したらここに1行足すだけで全機能に反映される。
	// SkyCylinderは全天を覆うため、Cameraは実体が見えないためクリック選択の対象外
	editorObjects_.clear();
	editorObjects_.push_back({ "Sphere", &object3d_->GetTransform(), true });
	editorObjects_.push_back({ "SkyCylinder", &skyCylinder_->GetTransform(), false });
	editorObjects_.push_back({ "Camera", &camera_->GetTransform(), false });
	editorObjects_.back().scaleEditable = false; // カメラのscaleはビュー行列を歪ませるため編集させない

#ifdef USE_IMGUI
	// 型別のInspector追加UI(ImGui呼び出しを含むためDebug構成のみ)
	editorObjects_.back().drawInspector = [this]() {
		float fovY = camera_->GetFovY();
		if (ImGui::DragFloat("FovY", &fovY, 0.01f)) {
			camera_->SetFovY(fovY);
		}
	};
#endif
	fixedEditorObjectCount_ = editorObjects_.size();

	// ステージ分を後ろへ連結(transformはStageData直指し。編集はStage::Updateが実体へ反映する)
	std::vector<EditorObject> stageObjects = stage_->BuildEditorObjects();
	editorObjects_.insert(editorObjects_.end(),
		std::make_move_iterator(stageObjects.begin()), std::make_move_iterator(stageObjects.end()));

#ifdef USE_IMGUI
	// ステージ分にはモデル差し替えComboと無効フラグ切替を付ける
	// (StageはImGui非依存のまま、UIは登録側=シーンが持つ)。
	// どちらも構造変更ではないためこの一覧は作り直し不要で、選択もそのまま維持される
	for (size_t i = fixedEditorObjectCount_; i < editorObjects_.size(); ++i) {
		const size_t stageObjectIndex = i - fixedEditorObjectCount_;
		editorObjects_[i].onSetDisabled = [this, i, stageObjectIndex](bool disabled) {
			stage_->SetObjectDisabled(stageObjectIndex, disabled);
			editorObjects_[i].pickable = !disabled; // 実体が消えるのでクリック選択の対象からも外す
		};
		editorObjects_[i].drawInspector = [this, stageObjectIndex]() {
			// 参照ではなくコピーで持つ(SetObjectModelが元の文字列を書き換えるため)
			const std::string current = stage_->GetData().objects[stageObjectIndex].model;
			if (ImGui::BeginCombo("Model", current.c_str())) {
				for (const std::string& file : modelFiles_) {
					if (ImGui::Selectable(file.c_str(), file == current) && file != current) {
						stage_->SetObjectModel(stageObjectIndex, file);
					}
				}
				ImGui::EndCombo();
			}
		};
	}
#endif

	// 一覧が変わったので選択は解除(古いindexは別物を指しうる)
	selectedIndex_ = -1;
}

Camera* GamePlayScene::GetActiveCamera() const {
	return debugCamera_->IsActive() ? debugCamera_->GetCamera() : camera_.get();
}

#ifdef USE_IMGUI
void GamePlayScene::RescanModelFiles() {
	// 再スキャンで一覧の並びが変わっても同じモデルを指し続けるよう、選択はindexではなくパスで引き継ぐ
	std::string selectedPath = "sphere.obj"; // 初回の既定
	if (addModelIndex_ >= 0 && addModelIndex_ < static_cast<int>(modelFiles_.size())) {
		selectedPath = modelFiles_[addModelIndex_];
	}

	modelFiles_ = ModelManager::GetInstance()->ScanModelFiles();

	addModelIndex_ = 0;
	for (int i = 0; i < static_cast<int>(modelFiles_.size()); ++i) {
		if (modelFiles_[i] == selectedPath) {
			addModelIndex_ = i;
			break;
		}
	}
}
#endif

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;