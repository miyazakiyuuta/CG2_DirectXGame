#pragma once
#include "stage/StageData.h"
#include "scene/EditorObject.h"

#include <memory>
#include <string>
#include <vector>

class Camera;
class Object3d;

// ステージのランタイム実体。StageDataを元にObject3d群を構築・描画する。
// 「Reload = ファイル読み + Rebuild / (将来の)Play in Editor = Capture + Rebuild」を
// 同一経路にするため、ファイル読み込みとデータからの構築(Rebuild)を分離している。
class Stage {
public:
	/// <summary>
	/// stage.jsonを読み込み、Object3d群を構築する
	/// </summary>
	/// <param name="path">作業ディレクトリ(プロジェクト直下)からの相対パス</param>
	/// <returns>読み込めたら true(失敗時は現在の状態を保つ)</returns>
	bool LoadFromFile(const std::string& path);

	// 保持中のStageDataからObject3d群を作り直す(Reload/Play in Editorの共通経路)
	void Rebuild();

	// --- エディタ用のデータ操作 ---
	// いずれも data_.objects を構造変更して Rebuild() まで行う(実体との整合を崩さない)。
	// 描画コマンド記録後(ImGuiのボタン処理等)に呼んでも安全。Rebuildは古い実体を即deleteせず
	// graveyard_へ退避し、次のUpdate冒頭で破棄する(記録済みコマンドの参照を守る遅延削除)。
	// 呼び出し側は BuildEditorObjects() の一覧を必ず作り直すこと(Transformポインタが無効になる)

	/// <summary>
	/// オブジェクトを末尾に追加する。nameは既存と重複しないよう採番される
	/// </summary>
	/// <returns>追加したオブジェクトの添字</returns>
	size_t AddObject(StageData::ObjectData objectData);

	/// <summary>
	/// index番のオブジェクトを複製し、Hierarchy上で隣に並ぶよう直後へ挿入する(nameは採番)
	/// </summary>
	/// <returns>複製したオブジェクトの添字(index+1)</returns>
	size_t DuplicateObject(size_t index);

	/// <summary>
	/// index番のオブジェクトを削除する
	/// </summary>
	void RemoveObject(size_t index);

	/// <summary>
	/// index番のオブジェクトのモデルを差し替える。構造変更(要素の追加/削除)ではないため、
	/// 上記と異なりBuildEditorObjects()の一覧は作り直さなくてよい(Transformポインタは有効なまま)
	/// </summary>
	void SetObjectModel(size_t index, const std::string& model);

	void Update(float deltaTime);
	void Draw();

	// 描画に使うカメラを差し替える(構築済みの全オブジェクトへも反映)
	void SetCamera(Camera* camera);

	/// <summary>
	/// Hierarchy/Inspector/ギズモ用の一覧を作る。
	/// transformはdata_.objectsの要素を直接指す(StageDataが唯一の編集対象)ため、
	/// data_.objectsを構造変更(追加/削除/Reload)したら必ず作り直すこと
	/// </summary>
	std::vector<EditorObject> BuildEditorObjects();

	StageData& GetData() { return data_; }
	const StageData& GetData() const { return data_; }

	Stage();
	~Stage();

private:
	// baseが既存オブジェクト名と重複する場合、"base_1" "base_2" と加算して空き名を返す
	std::string MakeUniqueName(const std::string& base) const;

	StageData data_;
	Camera* camera_ = nullptr;
	// data_.objectsと同じ並びのランタイム実体(disabledの要素はnullptr。indexで対応が取れる)
	std::vector<std::unique_ptr<Object3d>> objects_;
	// Rebuildで置き換えられた古い実体の待機場所。フレーム内で記録済みのコマンドが
	// まだ参照している可能性があるため、次のUpdate冒頭(前フレームのGPU実行完了後)まで生かす
	std::vector<std::unique_ptr<Object3d>> graveyard_;
};
