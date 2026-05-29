#include "LightManager.h"

#include <../externals/nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>

LightManager* LightManager::instance_ = nullptr;

namespace {

	float ClampMin(float value, float minValue)
	{
		return value < minValue ? minValue : value;
	}

	Vector4 ReadVector4(
		const nlohmann::json& json,
		const char* key,
		const Vector4& fallback
	)
	{
		if (!json.contains(key) || !json[key].is_array() || json[key].size() < 4) {
			return fallback;
		}

		return {
			json[key][0].get<float>(),
			json[key][1].get<float>(),
			json[key][2].get<float>(),
			json[key][3].get<float>()
		};
	}

	Vector3 ReadVector3(
		const nlohmann::json& json,
		const char* key,
		const Vector3& fallback
	)
	{
		if (!json.contains(key) || !json[key].is_array() || json[key].size() < 3) {
			return fallback;
		}

		return {
			json[key][0].get<float>(),
			json[key][1].get<float>(),
			json[key][2].get<float>()
		};
	}

	LightManager::EditablePointLight CreateFallbackGameplayLight()
	{
		LightManager::EditablePointLight light;

		light.name = "DefaultGameplayLight";
		light.enabled = true;
		light.color = { 1.0f, 1.0f, 1.0f, 1.0f };

		// いまGamePlaySceneに直書きしている値に近い保険ライト
		light.position = { 0.0f, -0.1f, 0.0f };
		light.intensity = 0.12f;
		light.radius = 20000.0f;
		light.decay = 0.0f;

		return light;
	}

	LightManager::EditablePointLight CreateEditorDefaultLight(size_t index)
	{
		LightManager::EditablePointLight light;

		light.name = "PointLight" + std::to_string(index);
		light.enabled = true;
		light.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		light.position = { 0.0f, 5.0f, 0.0f };
		light.intensity = 1.0f;
		light.radius = 30.0f;
		light.decay = 1.0f;

		return light;
	}

} // namespace

LightManager* LightManager::GetInstance()
{
	if (!instance_) {
		instance_ = new LightManager();
	}
	return instance_;
}

void LightManager::Clear()
{
	pointLights_.clear();
	ApplyToObject3dCommon();
}

void LightManager::SetFilePath(const std::string& filePath)
{
	filePath_ = filePath;
}

bool LightManager::LoadFromFile(const std::string& filePath)
{
	SetFilePath(filePath);

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		pointLights_.clear();
		pointLights_.push_back(CreateFallbackGameplayLight());
		ApplyToObject3dCommon();
		return false;
	}

	try {
		nlohmann::json root;
		ifs >> root;

		pointLights_.clear();

		if (root.contains("pointLights") && root["pointLights"].is_array()) {
			for (const auto& jsonLight : root["pointLights"]) {
				if (pointLights_.size() >= kMaxPointLights) {
					break;
				}

				EditablePointLight light;

				light.name = jsonLight.value("name", "PointLight");
				light.enabled = jsonLight.value("enabled", true);

				light.color = ReadVector4(
					jsonLight,
					"color",
					{ 1.0f, 1.0f, 1.0f, 1.0f }
				);

				light.position = ReadVector3(
					jsonLight,
					"position",
					{ 0.0f, 5.0f, 0.0f }
				);

				light.intensity = ClampMin(jsonLight.value("intensity", 1.0f), 0.0f);
				light.radius = ClampMin(jsonLight.value("radius", 30.0f), 0.001f);
				light.decay = ClampMin(jsonLight.value("decay", 1.0f), 0.0f);

				pointLights_.push_back(light);
			}
		}

		ApplyToObject3dCommon();
		return true;
	}
	catch (...) {
		pointLights_.clear();
		pointLights_.push_back(CreateFallbackGameplayLight());
		ApplyToObject3dCommon();
		return false;
	}
}

bool LightManager::SaveToFile(const std::string& filePath) const
{
	try {
		std::filesystem::path path(filePath);
		if (!path.parent_path().empty()) {
			std::filesystem::create_directories(path.parent_path());
		}

		nlohmann::json root;
		root["version"] = 1;
		root["pointLights"] = nlohmann::json::array();

		for (const auto& light : pointLights_) {
			nlohmann::json jsonLight;

			jsonLight["name"] = light.name;
			jsonLight["enabled"] = light.enabled;

			jsonLight["color"] = {
				light.color.x,
				light.color.y,
				light.color.z,
				light.color.w
			};

			jsonLight["position"] = {
				light.position.x,
				light.position.y,
				light.position.z
			};

			jsonLight["intensity"] = light.intensity;
			jsonLight["radius"] = light.radius;
			jsonLight["decay"] = light.decay;

			root["pointLights"].push_back(jsonLight);
		}

		std::ofstream ofs(filePath);
		if (!ofs.is_open()) {
			return false;
		}

		ofs << std::setw(2) << root << std::endl;
		return true;
	}
	catch (...) {
		return false;
	}
}

bool LightManager::AddDefaultPointLight()
{
	if (pointLights_.size() >= kMaxPointLights) {
		return false;
	}

	pointLights_.push_back(CreateEditorDefaultLight(pointLights_.size()));
	ApplyToObject3dCommon();

	return true;
}

void LightManager::RemovePointLight(size_t index)
{
	if (index >= pointLights_.size()) {
		return;
	}

	pointLights_.erase(pointLights_.begin() + index);
	ApplyToObject3dCommon();
}

void LightManager::MovePointLightUp(size_t index)
{
	if (index == 0 || index >= pointLights_.size()) {
		return;
	}

	std::swap(pointLights_[index], pointLights_[index - 1]);
	ApplyToObject3dCommon();
}

void LightManager::MovePointLightDown(size_t index)
{
	if (index + 1 >= pointLights_.size()) {
		return;
	}

	std::swap(pointLights_[index], pointLights_[index + 1]);
	ApplyToObject3dCommon();
}

void LightManager::ApplyToObject3dCommon() const
{
	PointLightArray array = {};
	array.count = 0;

	for (const auto& editableLight : pointLights_) {
		if (!editableLight.enabled) {
			continue;
		}

		if (array.count >= kMaxPointLights) {
			break;
		}

		PointLight& dst = array.lights[array.count];

		dst.color = editableLight.color;
		dst.position = editableLight.position;
		dst.intensity = ClampMin(editableLight.intensity, 0.0f);
		dst.radius = ClampMin(editableLight.radius, 0.001f);
		dst.decay = ClampMin(editableLight.decay, 0.0f);

		++array.count;
	}

	Object3dCommon::GetInstance()->SetPointLights(array);
}