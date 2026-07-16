#include "stage/StageSerializer.h"

#include "math/Serialization.h"
#include "utility/Logger.h"

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
