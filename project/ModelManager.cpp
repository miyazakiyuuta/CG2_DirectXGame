#include "ModelManager.h"
#include "Model.h"
#include "ModelCommon.h"
#include "SrvManager.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance() {
	if (instance == nullptr) {
		instance = new ModelManager();
	}
	return instance;
}

void ModelManager::Finalize() {
}

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Initialize(dxCommon, srvManager);
}

void ModelManager::LoadModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models_.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(modelCommon_.get(), "resources", filePath);

	// モデルをmapコンテナに収納する
	models_.insert(std::make_pair(filePath, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath) {
	// 読み込み済みモデルを検索
	if (models_.contains(filePath)) {
		// 読み込みモデルを戻り値としてreturn
		return models_.at(filePath).get();
	}
	// ファイル名一致なし
	return nullptr;
}
