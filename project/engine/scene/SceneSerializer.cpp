#include "scene/SceneSerializer.h"

#include "math/Serialization.h"
#include "utility/Logger.h"

#include <filesystem>
#include <fstream>

using json = nlohmann::json;

bool SceneSerializer::Save(const std::string& path, const std::vector<Entry>& entries) {
	// {"objects":[{"name":..., "transform":{...}}, ...]} の形に組み立てる
	// 将来、敵配置などを足すときは各要素に "type" フィールドを追加して拡張する
	json root;
	root["objects"] = json::array();
	for (const Entry& entry : entries) {
		if (!entry.transform) {
			continue;
		}
		root["objects"].push_back({
			{ "name", entry.name },
			{ "transform", *entry.transform }, // Serialization.h の to_json が自動で効く
		});
	}

	// 保存先ディレクトリが無ければ作る(初回Save対策)
	std::filesystem::path filePath(path);
	if (filePath.has_parent_path()) {
		std::error_code ec;
		std::filesystem::create_directories(filePath.parent_path(), ec);
	}

	std::ofstream file(path);
	if (!file) {
		Logger::Log("SceneSerializer::Save: failed to open: " + path + "\n");
		return false;
	}
	file << root.dump(4); // インデント付き(人間が読める形式)で書き出す
	Logger::Log("SceneSerializer::Save: saved: " + path + "\n");
	return true;
}

bool SceneSerializer::Load(const std::string& path, const std::vector<Entry>& entries) {
	std::ifstream file(path);
	if (!file) {
		// ファイルが無いのは初回起動の正常系なのでエラー扱いにしない
		return false;
	}

	try {
		json root = json::parse(file);
		for (const json& object : root.at("objects")) {
			const std::string name = object.at("name").get<std::string>();
			for (const Entry& entry : entries) {
				if (entry.transform && entry.name == name) {
					object.at("transform").get_to(*entry.transform);
				}
			}
		}
	} catch (const json::exception& e) {
		// 壊れたJSON・キー欠損でも落とさずログだけ残す(手編集ミス対策)
		Logger::Log("SceneSerializer::Load: parse error: " + path + ": " + e.what() + "\n");
		return false;
	}
	Logger::Log("SceneSerializer::Load: loaded: " + path + "\n");
	return true;
}
