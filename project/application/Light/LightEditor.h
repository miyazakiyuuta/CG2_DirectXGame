#pragma once

#include <string>

class LightEditor {
public:
	LightEditor() = default;
	~LightEditor() = default;

	void Initialize(const std::string& filePath);
	void DrawImGui();

private:
	void SetMessage(const std::string& message);

private:
	std::string filePath_ = "resources/stage_lights.json";
	char filePathBuffer_[256] = {};

	std::string message_;
};