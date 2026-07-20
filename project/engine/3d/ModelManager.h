#pragma once
#include "base/DirectXCommon.h"
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <DirectXPackedVector.h>

class Model;
class ModelCommon;
class SrvManager;

// モデルマネージャー
class ModelManager {
public:
	// シングルトンインスタンスの取得
	static ModelManager* GetInstance();
	// 終了
	void Finalize();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// モデルファイルの読み込み
	/// </summary>
	/// <param name="filePath">resources からの相対パス。サブフォルダは "human/human_re.gltf" のように指定する</param>
	void LoadModel(const std::string& filePath);

	/// <summary>
	/// モデルの検索
	/// </summary>
	/// <param name="filePath">モデルファイルのパス</param>
	/// <returns></returns>
	Model* FindModel(const std::string& filePath);

	/// <summary>
	/// resources配下のモデルファイル(.obj/.gltf)を列挙する(エディタのモデル選択UI用)
	/// </summary>
	/// <returns>LoadModelへそのまま渡せる相対パスのソート済み一覧(例:"fence/fence.obj")</returns>
	std::vector<std::string> ScanModelFiles() const;

private:
	static ModelManager* instance;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models_;

	std::unique_ptr<ModelCommon> modelCommon_ = nullptr;
};

