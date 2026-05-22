#include "ResultUI.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "UI/RuntimeTextTextureGenerator.h"
#include "base/WinApp.h"
#include "io/Input.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {
std::string ToUtf8String(const char* text) { return text ? std::string(text) : std::string(); }
#if defined(__cpp_char8_t)
std::string ToUtf8String(const char8_t* text) { return text ? std::string(reinterpret_cast<const char*>(text)) : std::string(); }
#endif
} // namespace

void ResultUI::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;
	state_ = State::None;
	isTitleRequested_ = false;
}

void ResultUI::TriggerClear(float clearTimeSeconds) {
	state_ = State::Clear;
	effectAlpha_ = 1.0f;
	isTitleRequested_ = false;

	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	TextureManager::GetInstance()->LoadTexture("white");
	bgSprite_ = std::make_unique<Sprite>();
	bgSprite_->Initialize(spriteCommon_, "white");
	bgSprite_->SetSize({screenW, screenH});
	bgSprite_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

	textTitleSprite_ = CreateTextSprite(ToUtf8String(u8"クリア！"), "resources/generated_ui/res_clear.png", screenH * 0.3f, {1.0f, 0.9f, 0.2f, 1.0f}, 140);

	int minutes = static_cast<int>(clearTimeSeconds) / 60;
	int seconds = static_cast<int>(clearTimeSeconds) % 60;
	std::ostringstream timeOss;
	timeOss << "TIME  " << std::setfill('0') << std::setw(2) << minutes << ":" << std::setw(2) << seconds;

	textTimeSprite_ = CreateTextSprite(timeOss.str(), "resources/generated_ui/res_time.png", screenH * 0.5f, {1.0f, 1.0f, 1.0f, 1.0f}, 80);
	textGuidanceSprite_ = CreateTextSprite(ToUtf8String(u8"スペースキーでタイトルへ"), "resources/generated_ui/res_guide.png", screenH * 0.8f, {0.8f, 0.8f, 0.8f, 1.0f}, 40);
}

void ResultUI::TriggerGameOver() {
	state_ = State::GameOver;
	effectAlpha_ = 1.0f;
	isTitleRequested_ = false;

	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	TextureManager::GetInstance()->LoadTexture("white");
	bgSprite_ = std::make_unique<Sprite>();
	bgSprite_->Initialize(spriteCommon_, "white");
	bgSprite_->SetSize({screenW, screenH});
	bgSprite_->SetColor({0.0f, 0.0f, 0.0f, 1.0f});

	textTitleSprite_ = CreateTextSprite(ToUtf8String(u8"LOSER..."), "resources/generated_ui/res_loser.png", screenH * 0.4f, {1.0f, 0.2f, 0.2f, 1.0f}, 160);
	textTimeSprite_ = nullptr;
	textGuidanceSprite_ = CreateTextSprite(ToUtf8String(u8"スペースキーでタイトルへ"), "resources/generated_ui/res_guide_over.png", screenH * 0.7f, {0.6f, 0.6f, 0.6f, 1.0f}, 40);
}

void ResultUI::Update() {
	if (state_ == State::None)
		return;

	if (state_ == State::Clear) {
		effectAlpha_ = (std::max)(0.2f, effectAlpha_ - 0.02f);
		if (bgSprite_) {
			bgSprite_->SetColor({1.0f, 1.0f, 1.0f, effectAlpha_});
			bgSprite_->Update();
		}
	} else if (state_ == State::GameOver) {
		effectAlpha_ = (std::max)(0.6f, effectAlpha_ - 0.015f);
		if (bgSprite_) {
			bgSprite_->SetColor({0.1f, 0.0f, 0.0f, effectAlpha_});
			bgSprite_->Update();
		}
	}

	if (textTitleSprite_)
		textTitleSprite_->Update();
	if (textTimeSprite_)
		textTimeSprite_->Update();

	bool canInput = (state_ == State::Clear && effectAlpha_ <= 0.2f) || (state_ == State::GameOver && effectAlpha_ <= 0.6f);

	if (canInput) {
		if (textGuidanceSprite_)
			textGuidanceSprite_->Update();
		if (Input::GetInstance()->IsTriggerKey(DIK_SPACE)) {
			isTitleRequested_ = true;
		}
	}
}

void ResultUI::Draw() {
	if (state_ == State::None)
		return;

	spriteCommon_->CommonDrawSetting();

	if (bgSprite_)
		bgSprite_->Draw();

	if ((state_ == State::Clear && effectAlpha_ < 0.8f) || (state_ == State::GameOver && effectAlpha_ < 0.9f)) {
		if (textTitleSprite_)
			textTitleSprite_->Draw();
		if (textTimeSprite_)
			textTimeSprite_->Draw();
	}

	bool canInput = (state_ == State::Clear && effectAlpha_ <= 0.2f) || (state_ == State::GameOver && effectAlpha_ <= 0.6f);
	if (canInput) {
		if (textGuidanceSprite_)
			textGuidanceSprite_->Draw();
	}
}

std::unique_ptr<Sprite> ResultUI::CreateTextSprite(const std::string& textUtf8, const std::string& outputPath, float yPos, const Vector4& color, int fontSize) {
	RuntimeTextTextureGenerator::GenerateDesc desc;
	desc.textUtf8 = textUtf8;
	desc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
	desc.outputFilePath = outputPath;
	desc.fontPixelSize = fontSize;
	desc.textColor = color;
	desc.shadowColor = {0.1f, 0.1f, 0.1f, 0.8f};
	desc.shadowOffsetX = 4;
	desc.shadowOffsetY = 4;

	if (!RuntimeTextTextureGenerator::GeneratePng(desc)) {
		return nullptr;
	}

	TextureManager::GetInstance()->LoadTexture(outputPath);
	const DirectX::TexMetadata& meta = TextureManager::GetInstance()->GetMetaData(outputPath);

	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon_, outputPath);
	sprite->SetAnchorPoint({0.5f, 0.5f});

	float screenW = static_cast<float>(WinApp::kClientWidth);
	sprite->SetPos({screenW * 0.5f, yPos});

	float drawScale = 0.5f;
	sprite->SetSize({static_cast<float>(meta.width) * drawScale, static_cast<float>(meta.height) * drawScale});
	sprite->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

	return sprite;
}