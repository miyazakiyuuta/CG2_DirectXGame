#pragma once

#include "3d/Object3dCommon.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

#include <cstddef>
#include <string>
#include <vector>

class LightManager {
public:
	struct EditablePointLight {
		std::string name = "PointLight";
		bool enabled = true;

		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		Vector3 position = { 0.0f, 5.0f, 0.0f };

		float intensity = 1.0f;
		float radius = 30.0f;
		float decay = 1.0f;
	};

public:
	static LightManager* GetInstance();

	void Clear();

	void SetFilePath(const std::string& filePath);
	const std::string& GetFilePath() const { return filePath_; }

	bool LoadFromFile(const std::string& filePath);
	bool SaveToFile(const std::string& filePath) const;

	bool AddDefaultPointLight();
	void RemovePointLight(size_t index);
	void MovePointLightUp(size_t index);
	void MovePointLightDown(size_t index);

	std::vector<EditablePointLight>& GetPointLightsForEdit() { return pointLights_; }
	const std::vector<EditablePointLight>& GetPointLights() const { return pointLights_; }

	void ApplyToObject3dCommon() const;

private:
	LightManager() = default;
	~LightManager() = default;

	LightManager(const LightManager&) = delete;
	LightManager& operator=(const LightManager&) = delete;

private:
	static LightManager* instance_;

	std::vector<EditablePointLight> pointLights_;
	std::string filePath_ = "resources/stage_lights.json";
};