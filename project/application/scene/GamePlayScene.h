#pragma once
#include "scene/BaseScene.h"
#include "scene/EditorObject.h"
#include "scene/SceneSerializer.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>

class Camera;
class DebugCamera;
class Object3d;
class Stage;
class Skybox;
class DebugGrid;
class GPUParticleEmitter;
class ParticleEmitter;
class SkyCylinder;

class GamePlayScene : public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	void DrawImGui() override;

	GamePlayScene();

	~GamePlayScene() override;

private:
	// editorObjects_からシーン保存/読込の対象一覧を作る
	std::vector<SceneSerializer::Entry> BuildSerializeEntries() const;

	// 描画に使うカメラ(デバッグカメラON中はそちらを返す)
	Camera* GetActiveCamera() const;

	// Hierarchy/Inspector/ギズモ/保存読込が共有するオブジェクト一覧と選択状態
	std::vector<EditorObject> editorObjects_;
	int selectedIndex_ = -1; // -1 = 選択なし

	std::unique_ptr<Camera> camera_ = nullptr;
	std::unique_ptr<DebugCamera> debugCamera_;

	std::unique_ptr<Object3d> object3d_;

	// stage.jsonから構築するステージ配置(静的オブジェクト群)
	std::unique_ptr<Stage> stage_;

	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<GPUParticleEmitter> gpuParticleEmitter_;

	std::unique_ptr<ParticleEmitter> sparkEmitter_;

	std::unique_ptr<SkyCylinder> skyCylinder_;
};

