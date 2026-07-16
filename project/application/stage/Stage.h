#pragma once
#include "stage/StageData.h"

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

	void Update(float deltaTime);
	void Draw();

	// 描画に使うカメラを差し替える(構築済みの全オブジェクトへも反映)
	void SetCamera(Camera* camera);

	StageData& GetData() { return data_; }
	const StageData& GetData() const { return data_; }

	Stage();
	~Stage();

private:
	StageData data_;
	Camera* camera_ = nullptr;
	// data_.objectsと同じ並びのランタイム実体(disabledの要素はnullptr。indexで対応が取れる)
	std::vector<std::unique_ptr<Object3d>> objects_;
};
