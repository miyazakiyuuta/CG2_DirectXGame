#include "ModelManager.h"
#include "Model.h"
#include "ModelCommon.h"
#include "SrvManager.h"

ModelManager* ModelManager::instance_ = nullptr;

ModelManager* ModelManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new ModelManager();
	}
	return instance_;
}

void ModelManager::Finalize() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	modelCommon_ = new ModelCommon();
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
	model->Initialize(modelCommon_, "resources", filePath);

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
