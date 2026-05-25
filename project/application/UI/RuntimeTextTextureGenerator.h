#pragma once

#include <string>

#include "math/Vector4.h"

class RuntimeTextTextureGenerator {
public:
	struct GenerateDesc {
		std::string textUtf8;
		std::string fontFilePath;
		std::string outputFilePath;

		bool overwriteIfExists = false;

		int fontPixelSize = 40;
		int paddingX = 24;
		int paddingY = 16;

		Vector4 textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 shadowColor = { 0.0f, 0.0f, 0.0f, 0.6f };
		int shadowOffsetX = 3;
		int shadowOffsetY = 3;
	};

public:
	static bool GeneratePng(const GenerateDesc& desc);

private:
	static bool EnsureGdiplusInitialized();

	static std::wstring Utf8ToWide(const std::string& textUtf8);
	static unsigned char ToColorByte(float value);
};