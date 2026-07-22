#include "stage/StageSerializer.h"

#include "math/Serialization.h"
#include "utility/Logger.h"

#include <filesystem>
#include <fstream>

using json = nlohmann::json;

bool StageSerializer::Load(const std::string& path, StageData& stageData) {
	std::ifstream file(path);
	if (!file) {
		// SceneSerializerと違い、stage.jsonはゲームの必須データなので無いのは異常系
		Logger::Log("StageSerializer::Load: failed to open: " + path + "\n");
		return false;
	}

	// 途中でパースエラーが起きた場合に半端な状態を残さないよう、
	// ローカルへ読み切ってから引数へ移す
	StageData loaded;
	try {
		json root = json::parse(file);
		for (const json& object : root.at("objects")) {
			const std::string type = object.at("type").get<std::string>();
			// 当面はstaticのみ対応。spawn(敵出現)等はレール実装時に追加する
			if (type != "static") {
				Logger::Log("StageSerializer::Load: unsupported type '" + type + "' (skipped)\n");
				continue;
			}

			StageData::ObjectData objectData;
			objectData.name = object.at("name").get<std::string>();
			objectData.model = object.at("model").get<std::string>();
			object.at("transform").get_to(objectData.transform); // Serialization.h のADLフックが効く

			// 以下は省略可能なフィールド(無ければ既定値のまま)
			if (object.contains("disabled")) {
				object.at("disabled").get_to(objectData.disabled);
			}
			if (object.contains("collider")) {
				const json& collider = object.at("collider");
				objectData.hasCollider = true;
				collider.at("type").get_to(objectData.collider.type);
				collider.at("center").get_to(objectData.collider.center);
				collider.at("size").get_to(objectData.collider.size);
			}

			loaded.objects.push_back(std::move(objectData));
		}

		// レール制御点(省略可能。無ければ空=レールなし)。
		// vector<Vector3>はnlohmannが要素ごとにSerialization.hのADLフックを呼ぶので変換コード不要
		if (root.contains("rail")) {
			root.at("rail").get_to(loaded.rail);
		}
	} catch (const json::exception& e) {
		// 壊れたJSON・キー欠損でも落とさずログだけ残す(手編集ミス対策)
		Logger::Log("StageSerializer::Load: parse error: " + path + ": " + e.what() + "\n");
		return false;
	}

	stageData = std::move(loaded);
	Logger::Log("StageSerializer::Load: loaded: " + path +
	            " (" + std::to_string(stageData.objects.size()) + " objects)\n");
	return true;
}

bool StageSerializer::Save(const std::string& path, const StageData& stageData) {
	json root;
	root["objects"] = json::array();
	for (const StageData::ObjectData& objectData : stageData.objects) {
		json object = {
			{ "type", "static" },
			{ "name", objectData.name },
			{ "model", objectData.model },
			{ "transform", objectData.transform }, // Serialization.h の to_json が自動で効く
		};
		// 省略可能フィールドは既定値なら書かない(Save→Loadのラウンドトリップでデータが一致する)
		if (objectData.disabled) {
			object["disabled"] = objectData.disabled;
		}
		if (objectData.hasCollider) {
			object["collider"] = {
				{ "type", objectData.collider.type },
				{ "center", objectData.collider.center },
				{ "size", objectData.collider.size },
			};
		}
		root["objects"].push_back(std::move(object));
	}

	// レール制御点も省略可能フィールドの流儀(既定値=空なら書かない)に合わせる
	if (!stageData.rail.empty()) {
		root["rail"] = stageData.rail;
	}

	// 保存先ディレクトリが無ければ作る(初回Save対策)
	std::filesystem::path filePath(path);
	if (filePath.has_parent_path()) {
		std::error_code ec;
		std::filesystem::create_directories(filePath.parent_path(), ec);
	}

	std::ofstream file(path);
	if (!file) {
		Logger::Log("StageSerializer::Save: failed to open: " + path + "\n");
		return false;
	}
	file << root.dump(4); // インデント付き(人間が読める形式)で書き出す
	Logger::Log("StageSerializer::Save: saved: " + path +
	            " (" + std::to_string(stageData.objects.size()) + " objects)\n");
	return true;
}
