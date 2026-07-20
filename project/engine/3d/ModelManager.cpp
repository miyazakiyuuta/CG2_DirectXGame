#include "3d/ModelManager.h"
#include "3d/Model.h"
#include "3d/ModelCommon.h"
#include "base/SrvManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance() {
	if (instance == nullptr) {
		instance = new ModelManager();
	}
	return instance;
}

void ModelManager::Finalize() {
	delete instance;
	instance = nullptr;
}

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Initialize(dxCommon, srvManager);
}

void ModelManager::LoadModel(const std::string& filePath) {
	// 読み込み済みモデルを検索。キーは resources からの相対パスそのものなので、
	// フォルダが違えば同名ファイルでも別キーになり衝突しない
	if (models_.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// "human/human_re.gltf" → directoryPath="resources/human", fileName="human_re.gltf"
	// "sphere.obj"          → directoryPath="resources",        fileName="sphere.obj"
	std::string directoryPath = "resources";
	std::string fileName = filePath;
	size_t slashPos = filePath.find_last_of('/');
	if (slashPos != std::string::npos) {                     // サブフォルダ指定があるとき
		directoryPath += "/" + filePath.substr(0, slashPos); // フォルダ部分を resources の後ろに足す
		fileName = filePath.substr(slashPos + 1);            // 最後の '/' より後ろがファイル名
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(modelCommon_.get(), directoryPath, fileName);

	// 相対パスをキーにしてmapコンテナに収納する
	models_.insert(std::make_pair(filePath, std::move(model)));
}

std::vector<std::string> ModelManager::ScanModelFiles() const {
	std::vector<std::string> files;

	// LoadModelの相対パス基準と同じ場所を走査する(作業ディレクトリ=プロジェクト直下が前提)
	const std::filesystem::path root = "resources";
	if (!std::filesystem::exists(root)) {
		return files;
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		// 拡張子は大文字小文字を区別せず判定する(".OBJ"のようなファイルも拾う)
		std::string extension = entry.path().extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (extension != ".obj" && extension != ".gltf") {
			continue;
		}

		// "resources\human\walk.gltf" → "human/walk.gltf"(LoadModelのキーと同じ区切り文字にする)
		files.push_back(std::filesystem::relative(entry.path(), root).generic_string());
	}

	std::sort(files.begin(), files.end());
	return files;
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
