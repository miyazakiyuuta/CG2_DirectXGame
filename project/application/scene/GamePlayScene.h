#pragma once
#include "scene/BaseScene.h"
#include "scene/EditorObject.h"
#include "scene/SceneSerializer.h"

#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

class ActionInput;
class Camera;
class DebugCamera;
class Object3d;
class Player;
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

	// 固定オブジェクト+ステージ分からeditorObjects_を作り直す(選択は解除される)。
	// ステージのデータを構造変更(追加/削除/Reload)した後は必ず呼ぶこと(Transformポインタが無効になるため)
	void RebuildEditorObjects();

	// 描画に使うカメラ(デバッグカメラON中はそちらを返す)
	Camera* GetActiveCamera() const;

	// stage.jsonのカメラ調整値(FovY/railSpeed/backDistance)をライブ値へ反映する(Apply)。
	// hasCameraがfalse(camera項目の無い旧stage.json)なら何もしない。
	// 初回LoadFromFile後とReload成功後に呼ぶ。逆方向(Capture)はSaveボタン側で行う
	void ApplyCameraFromStage();

#ifdef USE_IMGUI
	// resources配下のモデル一覧を取り直す(選択中のパスは一覧が変わっても可能なら維持する)
	void RescanModelFiles();
#endif

	// Hierarchy/Inspector/ギズモ/保存読込が共有するオブジェクト一覧と選択状態
	std::vector<EditorObject> editorObjects_;
	int selectedIndex_ = -1; // -1 = 選択なし
	// 先頭からこの個数分がC++直書きの固定オブジェクト(GamePlayScene.jsonの保存対象)。
	// 以降はステージ分(stage.jsonの管轄なのでシーン保存には含めない)
	size_t fixedEditorObjectCount_ = 0;

#ifdef USE_IMGUI
	// エディタのモデル選択Comboに出す一覧(resources配下のスキャン結果)と、Addで使う選択位置
	std::vector<std::string> modelFiles_;
	int addModelIndex_ = 0;
#endif

	std::unique_ptr<Camera> camera_ = nullptr;
	std::unique_ptr<DebugCamera> debugCamera_;

	// レールカメラ: レール上の現在距離[m]と速度[m/s]。
	// activeがfalseの間はゲームカメラのTransformを上書きしない(ギズモ/Inspectorでの編集用)
	float railDistance_ = 0.0f;
	float railSpeed_ = 10.0f;
	bool railCameraActive_ = true;

	// カメラをプレイヤーの何m後方に置くか。railDistance_はプレイヤーの進行度で、
	// カメラはプレイヤーの座標系(接線基準)を forward×この値 だけ下がった位置に派生させる。
	// レール基準点を1つに統一することで、カーブでも画面内の自機位置が固定される
	float cameraBackDistance_ = 10.0f;

	// 入力アクション層(KB/パッド→アクションの対応付け)とプレイヤー
	std::unique_ptr<ActionInput> actionInput_;
	std::unique_ptr<Player> player_;

	std::unique_ptr<Object3d> object3d_;

	// stage.jsonから構築するステージ配置(静的オブジェクト群)
	std::unique_ptr<Stage> stage_;

	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<GPUParticleEmitter> gpuParticleEmitter_;

	std::unique_ptr<ParticleEmitter> sparkEmitter_;

	std::unique_ptr<SkyCylinder> skyCylinder_;
};

