#include "stage/StageSerializer.h"

#include "math/Serialization.h"
#include "utility/Logger.h"

#include <algorithm>
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
			// 未知のtypeは読み飛ばす(将来の形式で保存されたファイルでも落とさない)
			if (type != "static" && type != "spawn" && type != "light") {
				Logger::Log("StageSerializer::Load: unsupported type '" + type + "' (skipped)\n");
				continue;
			}

			StageData::ObjectData objectData;
			if (type == "spawn") {
				objectData.type = StageData::ObjectType::Spawn;
			} else if (type == "light") {
				objectData.type = StageData::ObjectType::Light;
			} else {
				objectData.type = StageData::ObjectType::Static;
			}
			objectData.name = object.at("name").get<std::string>();
			// spawnは敵の種別からモデルが決まるためmodelを持たない(省略可)
			if (object.contains("model")) {
				object.at("model").get_to(objectData.model);
			}
			object.at("transform").get_to(objectData.transform); // Serialization.h のADLフックが効く

			// 敵の発生地点の情報(省略時は既定値のまま)
			if (object.contains("enemy")) {
				object.at("enemy").get_to(objectData.enemy);
			}
			if (object.contains("railDistance")) {
				object.at("railDistance").get_to(objectData.railDistance);
			}

			// 点光源の性質(省略時は既定値のまま。項目ごとに省略可)
			if (object.contains("light")) {
				const json& light = object.at("light");
				if (light.contains("color")) {
					light.at("color").get_to(objectData.light.color);
				}
				if (light.contains("intensity")) {
					light.at("intensity").get_to(objectData.light.intensity);
				}
				if (light.contains("radius")) {
					light.at("radius").get_to(objectData.light.radius);
				}
				if (light.contains("decay")) {
					light.at("decay").get_to(objectData.light.decay);
				}
			}

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

		// 区間(省略可能。無ければ空=ステージ全体が1区間)。
		// フィールドはcameraと同じく1個ずつ省略可(手編集ミス1項目で全体を失敗させない)
		if (root.contains("sections")) {
			for (const json& section : root.at("sections")) {
				StageData::SectionData sectionData;
				if (section.contains("name")) {
					section.at("name").get_to(sectionData.name);
				}
				if (section.contains("startDistance")) {
					section.at("startDistance").get_to(sectionData.startDistance);
				}
				loaded.sections.push_back(std::move(sectionData));
			}
			// 「開始距離の昇順・先頭は0」の約束(StageData::sections)に揃える。
			// エディタの操作はこの約束を守るので、崩れているのは手編集したときだけ。
			// 並び替えは同じ距離どうしの順を保つstable_sortにする(手で書いた順を勝手に入れ替えない)
			std::stable_sort(loaded.sections.begin(), loaded.sections.end(),
				[](const StageData::SectionData& a, const StageData::SectionData& b) {
					return a.startDistance < b.startDistance;
				});
			if (!loaded.sections.empty()) {
				loaded.sections.front().startDistance = 0.0f;
			}
		}

		// ステージ全体の光(省略可能。無ければhasLighting=falseで光は適用しない=昼の見た目のまま)。
		// 値はcameraと同じく1個ずつ省略可
		if (root.contains("lighting")) {
			const json& lighting = root.at("lighting");
			StageData::LightingData& data = loaded.lighting;
			loaded.hasLighting = true;
			if (lighting.contains("ambient")) {
				lighting.at("ambient").get_to(data.ambientColor);
			}
			if (lighting.contains("moon")) {
				const json& moon = lighting.at("moon");
				if (moon.contains("color")) {
					moon.at("color").get_to(data.moonColor);
				}
				if (moon.contains("direction")) {
					moon.at("direction").get_to(data.moonDirection);
				}
				if (moon.contains("intensity")) {
					moon.at("intensity").get_to(data.moonIntensity);
				}
			}
			if (lighting.contains("headlight")) {
				const json& headlight = lighting.at("headlight");
				if (headlight.contains("color")) {
					headlight.at("color").get_to(data.headlightColor);
				}
				if (headlight.contains("intensity")) {
					headlight.at("intensity").get_to(data.headlightIntensity);
				}
				if (headlight.contains("distance")) {
					headlight.at("distance").get_to(data.headlightDistance);
				}
				if (headlight.contains("angle")) {
					headlight.at("angle").get_to(data.headlightAngle);
				}
				if (headlight.contains("falloffStart")) {
					headlight.at("falloffStart").get_to(data.headlightFalloffStart);
				}
				if (headlight.contains("decay")) {
					headlight.at("decay").get_to(data.headlightDecay);
				}
			}
		}

		// カメラ調整値(省略可能。無ければhasCamera=falseでシーンはApplyしない)。
		// フィールドも1個ずつ省略可(無い項目は構造体デフォルトのまま)。colliderのat()一括より
		// 緩いのは、手編集ミス1項目で全体を失敗させない+将来のフィールド追加で旧ファイルを壊さないため
		if (root.contains("camera")) {
			const json& camera = root.at("camera");
			loaded.hasCamera = true;
			if (camera.contains("fovY")) {
				camera.at("fovY").get_to(loaded.camera.fovY);
			}
			if (camera.contains("railSpeed")) {
				camera.at("railSpeed").get_to(loaded.camera.railSpeed);
			}
			if (camera.contains("backDistance")) {
				camera.at("backDistance").get_to(loaded.camera.backDistance);
			}
			if (camera.contains("nearClip")) {
				camera.at("nearClip").get_to(loaded.camera.nearClip);
			}
			if (camera.contains("farClip")) {
				camera.at("farClip").get_to(loaded.camera.farClip);
			}
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
		const char* typeName = "static";
		if (objectData.type == StageData::ObjectType::Spawn) {
			typeName = "spawn";
		} else if (objectData.type == StageData::ObjectType::Light) {
			typeName = "light";
		}
		json object = {
			{ "type", typeName },
			{ "name", objectData.name },
			{ "transform", objectData.transform }, // Serialization.h の to_json が自動で効く
		};
		if (objectData.type == StageData::ObjectType::Spawn) {
			// 敵の種別と発生する進行度。モデルは種別から決まるので書かない
			object["enemy"] = objectData.enemy;
			object["railDistance"] = objectData.railDistance;
		} else if (objectData.type == StageData::ObjectType::Light) {
			// 点光源の性質。モデルは持たない(位置はtransform.translate)
			object["light"] = {
				{ "color", objectData.light.color },
				{ "intensity", objectData.light.intensity },
				{ "radius", objectData.light.radius },
				{ "decay", objectData.light.decay },
			};
		} else {
			object["model"] = objectData.model;
		}
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

	// 区間もレール制御点と同じく、空なら書かない
	if (!stageData.sections.empty()) {
		root["sections"] = json::array();
		for (const StageData::SectionData& sectionData : stageData.sections) {
			root["sections"].push_back({
				{ "name", sectionData.name },
				{ "startDistance", sectionData.startDistance },
			});
		}
	}

	// ステージ全体の光(hasLightingのときだけ書く。カメラと同じくラウンドトリップでデータが一致する)
	if (stageData.hasLighting) {
		const StageData::LightingData& lighting = stageData.lighting;
		root["lighting"] = {
			{ "ambient", lighting.ambientColor },
			{ "moon", {
				{ "color", lighting.moonColor },
				{ "direction", lighting.moonDirection },
				{ "intensity", lighting.moonIntensity },
			} },
			{ "headlight", {
				{ "color", lighting.headlightColor },
				{ "intensity", lighting.headlightIntensity },
				{ "distance", lighting.headlightDistance },
				{ "angle", lighting.headlightAngle },
				{ "falloffStart", lighting.headlightFalloffStart },
				{ "decay", lighting.headlightDecay },
			} },
		};
	}

	// カメラ調整値(hasCameraのときだけ書く。Save→Loadのラウンドトリップでデータが一致する)
	if (stageData.hasCamera) {
		root["camera"] = {
			{ "fovY", stageData.camera.fovY },
			{ "railSpeed", stageData.camera.railSpeed },
			{ "backDistance", stageData.camera.backDistance },
			{ "nearClip", stageData.camera.nearClip },
			{ "farClip", stageData.camera.farClip },
		};
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
