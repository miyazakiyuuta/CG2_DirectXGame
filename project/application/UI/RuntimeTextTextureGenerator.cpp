#include "UI/RuntimeTextTextureGenerator.h"

#include <Windows.h>
#include <gdiplus.h>
#include <memory>
#include <vector>
#include <filesystem>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

	bool g_gdiplusInitialized = false;
	ULONG_PTR g_gdiplusToken = 0;

	bool GetPngEncoderClsid(CLSID* clsid)
	{
		if (!clsid) {
			return false;
		}

		UINT num = 0;
		UINT size = 0;
		Gdiplus::GetImageEncodersSize(&num, &size);
		if (size == 0) {
			return false;
		}

		std::vector<unsigned char> buffer(size);
		auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());

		Gdiplus::GetImageEncoders(num, size, encoders);

		for (UINT i = 0; i < num; ++i) {
			if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
				*clsid = encoders[i].Clsid;
				return true;
			}
		}

		return false;
	}

} // namespace

bool RuntimeTextTextureGenerator::EnsureGdiplusInitialized()
{
	if (g_gdiplusInitialized) {
		return true;
	}

	Gdiplus::GdiplusStartupInput startupInput;
	Gdiplus::Status status = Gdiplus::GdiplusStartup(&g_gdiplusToken, &startupInput, nullptr);
	if (status != Gdiplus::Ok) {
		return false;
	}

	g_gdiplusInitialized = true;
	return true;
}



std::wstring RuntimeTextTextureGenerator::Utf8ToWide(const std::string& textUtf8)
{
	if (textUtf8.empty()) {
		return L"";
	}

	int requiredSize = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		textUtf8.data(),
		static_cast<int>(textUtf8.size()),
		nullptr,
		0
	);

	if (requiredSize <= 0) {
		return L"";
	}

	std::wstring result;
	result.resize(requiredSize);

	int convertedSize = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		textUtf8.data(),
		static_cast<int>(textUtf8.size()),
		result.data(),
		requiredSize
	);

	if (convertedSize <= 0) {
		return L"";
	}

	return result;
}

unsigned char RuntimeTextTextureGenerator::ToColorByte(float value)
{
	if (value < 0.0f) {
		value = 0.0f;
	}
	if (value > 1.0f) {
		value = 1.0f;
	}

	return static_cast<unsigned char>(value * 255.0f + 0.5f);
}

bool RuntimeTextTextureGenerator::GeneratePng(const GenerateDesc& desc)
{

	if (desc.outputFilePath.empty()) {
		return false;
	}

	// 既に生成済みなら、同じPNGを作り直さない
	if (!desc.overwriteIfExists &&
		std::filesystem::exists(desc.outputFilePath)) {
		return true;
	}


	if (!EnsureGdiplusInitialized()) {
		return false;
	}

	std::wstring text = Utf8ToWide(desc.textUtf8);
	std::wstring fontPath = Utf8ToWide(desc.fontFilePath);
	std::wstring outputPath = Utf8ToWide(desc.outputFilePath);

	if (text.empty() || fontPath.empty() || outputPath.empty()) {
		return false;
	}

	Gdiplus::PrivateFontCollection privateFonts;

	Gdiplus::Status addFontStatus = privateFonts.AddFontFile(fontPath.c_str());
	if (addFontStatus != Gdiplus::Ok) {
		return false;
	}

	int familyCount = privateFonts.GetFamilyCount();
	if (familyCount <= 0) {
		return false;
	}

	std::vector<Gdiplus::FontFamily> families(familyCount);
	int foundFamilyCount = 0;

	Gdiplus::Status getFamiliesStatus =
		privateFonts.GetFamilies(familyCount, families.data(), &foundFamilyCount);

	if (getFamiliesStatus != Gdiplus::Ok || foundFamilyCount <= 0) {
		return false;
	}

	Gdiplus::Font font(
		&families[0],
		static_cast<Gdiplus::REAL>(desc.fontPixelSize),
		Gdiplus::FontStyleRegular,
		Gdiplus::UnitPixel
	);

	if (font.GetLastStatus() != Gdiplus::Ok) {
		return false;
	}

	// まずサイズ測定
	Gdiplus::Bitmap measureBitmap(1, 1, PixelFormat32bppARGB);
	Gdiplus::Graphics measureGraphics(&measureBitmap);
	measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

	Gdiplus::RectF measureRect(0, 0, 4096.0f, 1024.0f);
	Gdiplus::RectF boundRect;
	measureGraphics.MeasureString(
		text.c_str(),
		-1,
		&font,
		measureRect,
		&boundRect
	);

	int imageWidth = static_cast<int>(boundRect.Width + 0.999f) + desc.paddingX * 2;
	int imageHeight = static_cast<int>(boundRect.Height + 0.999f) + desc.paddingY * 2;

	if (imageWidth <= 0) {
		imageWidth = 1;
	}
	if (imageHeight <= 0) {
		imageHeight = 1;
	}

	Gdiplus::Bitmap bitmap(imageWidth, imageHeight, PixelFormat32bppARGB);
	Gdiplus::Graphics graphics(&bitmap);

	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

	Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(
		ToColorByte(desc.shadowColor.w),
		ToColorByte(desc.shadowColor.x),
		ToColorByte(desc.shadowColor.y),
		ToColorByte(desc.shadowColor.z)
	));

	Gdiplus::SolidBrush textBrush(Gdiplus::Color(
		ToColorByte(desc.textColor.w),
		ToColorByte(desc.textColor.x),
		ToColorByte(desc.textColor.y),
		ToColorByte(desc.textColor.z)
	));

	Gdiplus::PointF shadowPos(
		static_cast<Gdiplus::REAL>(desc.paddingX + desc.shadowOffsetX),
		static_cast<Gdiplus::REAL>(desc.paddingY + desc.shadowOffsetY)
	);

	Gdiplus::PointF textPos(
		static_cast<Gdiplus::REAL>(desc.paddingX),
		static_cast<Gdiplus::REAL>(desc.paddingY)
	);

	graphics.DrawString(text.c_str(), -1, &font, shadowPos, &shadowBrush);
	graphics.DrawString(text.c_str(), -1, &font, textPos, &textBrush);

	CLSID pngClsid;
	if (!GetPngEncoderClsid(&pngClsid)) {
		return false;
	}

	std::filesystem::path outputFsPath(desc.outputFilePath);
	if (outputFsPath.has_parent_path()) {
		std::filesystem::create_directories(outputFsPath.parent_path());
	}

	Gdiplus::Status saveStatus = bitmap.Save(outputPath.c_str(), &pngClsid, nullptr);
	return saveStatus == Gdiplus::Ok;
}