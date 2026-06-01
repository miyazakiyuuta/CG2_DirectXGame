#include "ResultUI.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "UI/RuntimeTextTextureGenerator.h"
#include "base/WinApp.h"
#include "io/Input.h"
#include "scene/GameStartSettings.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <../externals/nlohmann/json.hpp>

namespace {
	std::string ToUtf8String(const char* text) { return text ? std::string(text) : std::string(); }
#if defined(__cpp_char8_t)
	std::string ToUtf8String(const char8_t* text) { return text ? std::string(reinterpret_cast<const char*>(text)) : std::string(); }
#endif
	constexpr const char* kLegacyClearTimeRankingFilePath = "resources/clear_time_ranking.json";
} // namespace

void ResultUI::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;
	state_ = State::None;
	isTitleRequested_ = false;
	isRestartRequested_ = false;
}

void ResultUI::TriggerClear(float clearTimeSeconds) {
	state_ = State::Clear;
	effectAlpha_ = 1.0f;
	isTitleRequested_ = false;
	isRestartRequested_ = false;

	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	TextureManager::GetInstance()->LoadTexture("white");
	bgSprite_ = std::make_unique<Sprite>();
	bgSprite_->Initialize(spriteCommon_, "white");
	bgSprite_->SetSize({ screenW, screenH });
	bgSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	textTitleSprite_ = CreateTextSprite(ToUtf8String(u8"クリア！"), "resources/generated_ui/res_clear.png", screenH * 0.125f, { 1.0f, 0.9f, 0.2f, 1.0f }, 200);

	clearTimeBlinkTimer_ = 0.0f;
	clearTimeRankingUpdated_ = UpdateClearTimeRanking(clearTimeSeconds);

	textTimeSprite_ = nullptr;
	CreateClearRankingSprites(clearTimeSeconds);

	textGuidanceSprite_ = CreateTextSprite(
		ToUtf8String(u8"スペースキーでタイトルへ"),
		"resources/generated_ui/res_guide.png",
		screenH * 0.88f,
		{ 0.8f, 0.8f, 0.8f, 1.0f },
		60
	);
	textGuidanceGamepadSprite_ = CreateTextSprite(
		ToUtf8String(u8"Aボタンでタイトルへ"),
		"resources/generated_ui/res_guide_gamepad.png",
		screenH * 0.88f,
		{ 0.8f, 0.8f, 0.8f, 1.0f },
		60
	);
	textRestartGuidanceSprite_ = CreateTextSprite(
		ToUtf8String(u8"Rキーでリスタート"),
		"resources/generated_ui/res_restart_guide.png",
		screenH * 0.94f,
		{ 0.8f, 0.8f, 0.8f, 1.0f },
		60
	);
	textRestartGuidanceGamepadSprite_ = CreateTextSprite(
		ToUtf8String(u8"Bボタンでリスタート"),
		"resources/generated_ui/res_restart_guide_gamepad.png",
		screenH * 0.94f,
		{ 0.8f, 0.8f, 0.8f, 1.0f },
		60
	);
}

void ResultUI::TriggerGameOver() {
	state_ = State::GameOver;
	effectAlpha_ = 1.0f;
	isTitleRequested_ = false;
	isRestartRequested_ = false;

	float screenW = static_cast<float>(WinApp::kClientWidth);
	float screenH = static_cast<float>(WinApp::kClientHeight);

	TextureManager::GetInstance()->LoadTexture("white");
	bgSprite_ = std::make_unique<Sprite>();
	bgSprite_->Initialize(spriteCommon_, "white");
	bgSprite_->SetSize({ screenW, screenH });
	bgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });

	textTitleSprite_ = CreateTextSprite(ToUtf8String(u8"LOSER..."), "resources/generated_ui/res_loser.png", screenH * 0.4f, { 1.0f, 0.2f, 0.2f, 1.0f }, 160);
	textTimeSprite_ = nullptr;
	textGuidanceSprite_ = CreateTextSprite(ToUtf8String(u8"スペースキーでタイトルへ"), "resources/generated_ui/res_guide_over.png", screenH * 0.7f, { 0.6f, 0.6f, 0.6f, 1.0f }, 80);
	textGuidanceGamepadSprite_ = CreateTextSprite(ToUtf8String(u8"Aボタンでタイトルへ"), "resources/generated_ui/res_guide_over_gamepad.png", screenH * 0.7f, { 0.6f, 0.6f, 0.6f, 1.0f }, 80);
	textRestartGuidanceSprite_ = CreateTextSprite(ToUtf8String(u8"Rキーでリスタート"), "resources/generated_ui/res_restart_guide_over.png", screenH * 0.78f, { 0.6f, 0.6f, 0.6f, 1.0f }, 80);
	textRestartGuidanceGamepadSprite_ = CreateTextSprite(ToUtf8String(u8"Bボタンでリスタート"), "resources/generated_ui/res_restart_guide_over_gamepad.png", screenH * 0.78f, { 0.6f, 0.6f, 0.6f, 1.0f }, 80);
}

void ResultUI::Update() {
	if (state_ == State::None)
		return;

	if (state_ == State::Clear) {
		effectAlpha_ = (std::max)(0.2f, effectAlpha_ - 0.02f);
		if (bgSprite_) {
			bgSprite_->SetColor({ 1.0f, 1.0f, 1.0f, effectAlpha_ });
			bgSprite_->Update();
		}
	}
	else if (state_ == State::GameOver) {
		effectAlpha_ = (std::max)(0.6f, effectAlpha_ - 0.015f);
		if (bgSprite_) {
			bgSprite_->SetColor({ 0.1f, 0.0f, 0.0f, effectAlpha_ });
			bgSprite_->Update();
		}
	}

	if (textTitleSprite_)
		textTitleSprite_->Update();
	if (textTimeSprite_)
		textTimeSprite_->Update();

	bool canInput = (state_ == State::Clear && effectAlpha_ <= 0.2f) || (state_ == State::GameOver && effectAlpha_ <= 0.6f);

	if (canInput) {
		Input* input = Input::GetInstance();
		
		if (input->IsControllerConnected()) {
			if (input->IsPressPad(XINPUT_GAMEPAD_A) || input->IsPressPad(XINPUT_GAMEPAD_B) || 
			    input->IsPressPad(XINPUT_GAMEPAD_X) || input->IsPressPad(XINPUT_GAMEPAD_Y) ||
			    (input->GetLeftStickX() * input->GetLeftStickX() + input->GetLeftStickY() * input->GetLeftStickY() > 0.05f)) {
				isGamepadMode_ = true;
			}
		}
		
		if (input->IsPushKey(DIK_SPACE) || input->IsPushKey(DIK_W) || input->IsPushKey(DIK_A) || 
		    input->IsPushKey(DIK_S) || input->IsPushKey(DIK_D)) {
			isGamepadMode_ = false;
		}

		if (isGamepadMode_) {
			if (textGuidanceGamepadSprite_)
				textGuidanceGamepadSprite_->Update();
			if (textRestartGuidanceGamepadSprite_)
				textRestartGuidanceGamepadSprite_->Update();
		} else {
			if (textGuidanceSprite_)
				textGuidanceSprite_->Update();
			if (textRestartGuidanceSprite_)
				textRestartGuidanceSprite_->Update();
		}

		if (input->IsTriggerKey(DIK_SPACE) || input->IsTriggerPad(XINPUT_GAMEPAD_A)) {
			isTitleRequested_ = true;
		}
		if (input->IsTriggerKey(DIK_R) || input->IsTriggerPad(XINPUT_GAMEPAD_B)) {
			isRestartRequested_ = true;
		}
	}
	if (rankingTitleSprite_) {
		rankingTitleSprite_->Update();
	}

	for (auto& sprite : rankingTimeSprites_) {
		if (sprite) {
			sprite->Update();
		}
	}

	if (currentTimeLabelSprite_) {
		currentTimeLabelSprite_->Update();
	}

	if (clearTimeRankingUpdated_ &&
		clearTimeRankingInsertedIndex_ >= 0 &&
		clearTimeRankingInsertedIndex_ < static_cast<int>(rankingTimeSprites_.size())) {

		clearTimeBlinkTimer_ += 1.0f / 60.0f;

		float blink =
			0.5f + 0.5f * std::sin(clearTimeBlinkTimer_ * 8.0f);

		for (int i = 0; i < static_cast<int>(rankingTimeSprites_.size()); ++i) {
			auto& sprite = rankingTimeSprites_[i];
			if (!sprite) {
				continue;
			}

			if (i == clearTimeRankingInsertedIndex_) {
				sprite->SetColor({
					1.0f,
					0.72f + 0.28f * blink,
					0.10f + 0.35f * blink,
					1.0f
					});
			}
			else {
				sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}

			sprite->Update();
		}
	}
	else {
		for (auto& sprite : rankingTimeSprites_) {
			if (sprite) {
				sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				sprite->Update();
			}
		}
	}

	if (currentTimeValueSprite_) {
		currentTimeValueSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		currentTimeValueSprite_->Update();
	}
}

void ResultUI::Draw() {
	if (state_ == State::None)
		return;

	spriteCommon_->CommonDrawSetting();

	if (bgSprite_)
		bgSprite_->Draw();

	if ((state_ == State::Clear && effectAlpha_ < 0.8f) || (state_ == State::GameOver && effectAlpha_ < 0.9f)) {
		if (state_ == State::GameOver) {
			if (textTitleSprite_)
				textTitleSprite_->Draw();
		}

		if (rankingTitleSprite_) {
			rankingTitleSprite_->Draw();
		}

		for (auto& sprite : rankingTimeSprites_) {
			if (sprite) {
				sprite->Draw();
			}
		}

		if (currentTimeLabelSprite_) {
			currentTimeLabelSprite_->Draw();
		}

		if (currentTimeValueSprite_) {
			currentTimeValueSprite_->Draw();
		}
	}

	bool canInput = (state_ == State::Clear && effectAlpha_ <= 0.2f) || (state_ == State::GameOver && effectAlpha_ <= 0.6f);
	if (canInput) {
		if (isGamepadMode_) {
			if (textGuidanceGamepadSprite_)
				textGuidanceGamepadSprite_->Draw();
			if (textRestartGuidanceGamepadSprite_)
				textRestartGuidanceGamepadSprite_->Draw();
		} else {
			if (textGuidanceSprite_)
				textGuidanceSprite_->Draw();
			if (textRestartGuidanceSprite_)
				textRestartGuidanceSprite_->Draw();
		}
	}
}

std::unique_ptr<Sprite> ResultUI::CreateTextSprite(const std::string& textUtf8, const std::string& outputPath, float yPos, const Vector4& color, int fontSize) {
	RuntimeTextTextureGenerator::GenerateDesc desc;
	desc.textUtf8 = textUtf8;
	desc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
	desc.outputFilePath = outputPath;
	desc.fontPixelSize = fontSize;
	desc.textColor = color;
	desc.shadowColor = { 0.1f, 0.1f, 0.1f, 0.8f };
	desc.shadowOffsetX = 4;
	desc.shadowOffsetY = 4;
	desc.overwriteIfExists = true;

	if (!RuntimeTextTextureGenerator::GeneratePng(desc)) {
		return nullptr;
	}

	// ランタイム生成テクスチャは毎回中身が変わるので、キャッシュを破棄して再読み込みする
	TextureManager::GetInstance()->ReloadTexture(outputPath);
	const DirectX::TexMetadata& meta = TextureManager::GetInstance()->GetMetaData(outputPath);

	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon_, outputPath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });

	float screenW = static_cast<float>(WinApp::kClientWidth);
	sprite->SetPos({ screenW * 0.5f, yPos });

	float drawScale = 0.5f;
	sprite->SetSize({ static_cast<float>(meta.width) * drawScale, static_cast<float>(meta.height) * drawScale });
	sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

	return sprite;
}

std::string ResultUI::FormatClearTime(float seconds) const
{
	if (seconds < 0.0f) {
		seconds = 0.0f;
	}

	int totalCentiseconds =
		static_cast<int>(std::round(seconds * 100.0f));

	int minutes = totalCentiseconds / (60 * 100);
	int rest = totalCentiseconds % (60 * 100);
	int sec = rest / 100;
	int centi = rest % 100;

	std::ostringstream oss;
	oss << std::setfill('0')
		<< std::setw(2) << minutes
		<< ":"
		<< std::setw(2) << sec
		<< "."
		<< std::setw(2) << centi;

	return oss.str();
}

std::vector<float> ResultUI::LoadClearTimeRanking() const
{
	std::vector<float> times;
	const std::string rankingFilePath = GetClearTimeRankingFilePath();
	std::string loadFilePath = rankingFilePath;

	try {
		if (!std::filesystem::exists(loadFilePath)) {
			if (GameStartSettings::GetDifficulty() == GameStartSettings::Difficulty::Normal &&
				std::filesystem::exists(kLegacyClearTimeRankingFilePath)) {
				loadFilePath = kLegacyClearTimeRankingFilePath;
			}
			else {
				return times;
			}
		}

		std::ifstream ifs(loadFilePath);
		if (!ifs.is_open()) {
			return times;
		}

		nlohmann::json j;
		ifs >> j;

		if (j.contains("times") && j["times"].is_array()) {
			for (const auto& item : j["times"]) {
				if (item.is_number()) {
					times.push_back(item.get<float>());
				}
			}
		}
	}
	catch (...) {
		times.clear();
	}

	std::sort(times.begin(), times.end());

	if (times.size() > kClearTimeRankingMaxCount_) {
		times.resize(kClearTimeRankingMaxCount_);
	}

	return times;
}

bool ResultUI::SaveClearTimeRanking(const std::vector<float>& times) const
{
	const std::string rankingFilePath = GetClearTimeRankingFilePath();

	try {
		std::filesystem::path path(rankingFilePath);
		if (path.has_parent_path()) {
			std::filesystem::create_directories(path.parent_path());
		}

		nlohmann::json j;
		j["version"] = 1;
		j["times"] = nlohmann::json::array();

		for (float t : times) {
			float rounded = std::round(t * 100.0f) / 100.0f;
			j["times"].push_back(rounded);
		}

		std::ofstream ofs(rankingFilePath);
		if (!ofs.is_open()) {
			return false;
		}

		ofs << std::setw(2) << j << std::endl;
		return true;
	}
	catch (...) {
		return false;
	}
}

std::string ResultUI::GetClearTimeRankingFilePath() const
{
	switch (GameStartSettings::GetDifficulty()) {
	case GameStartSettings::Difficulty::Normal:
		return "resources/clear_time_ranking_normal.json";
	case GameStartSettings::Difficulty::Hard:
		return "resources/clear_time_ranking_hard.json";
	case GameStartSettings::Difficulty::Hell:
		return "resources/clear_time_ranking_hell.json";
	default:
		return "resources/clear_time_ranking_normal.json";
	}
}

bool ResultUI::UpdateClearTimeRanking(float clearTimeSeconds)
{
	clearTimeRanking_ = LoadClearTimeRanking();
	clearTimeRankingInsertedIndex_ = -1;

	bool canInsert = false;

	if (clearTimeRanking_.size() < kClearTimeRankingMaxCount_) {
		canInsert = true;
	}
	else if (!clearTimeRanking_.empty() &&
		clearTimeSeconds < clearTimeRanking_.back()) {
		canInsert = true;
	}

	if (!canInsert) {
		return false;
	}

	// 同じタイムがある場合は、その後ろへ入れる
	auto insertIt = std::upper_bound(
		clearTimeRanking_.begin(),
		clearTimeRanking_.end(),
		clearTimeSeconds
	);

	clearTimeRankingInsertedIndex_ =
		static_cast<int>(std::distance(clearTimeRanking_.begin(), insertIt));

	clearTimeRanking_.insert(insertIt, clearTimeSeconds);

	if (clearTimeRanking_.size() > kClearTimeRankingMaxCount_) {
		clearTimeRanking_.resize(kClearTimeRankingMaxCount_);
	}

	if (clearTimeRankingInsertedIndex_ >= kClearTimeRankingMaxCount_) {
		clearTimeRankingInsertedIndex_ = -1;
		return false;
	}

	SaveClearTimeRanking(clearTimeRanking_);
	return true;
}

void ResultUI::CreateClearRankingSprites(float clearTimeSeconds)
{
	rankingTimeSprites_.clear();

	const float screenW = static_cast<float>(WinApp::kClientWidth);
	const float screenH = static_cast<float>(WinApp::kClientHeight);

	// -----------------------------
	// レイアウト調整用
	// -----------------------------
	const float rankingX = screenW * 0.26f;
	const float currentTimeX = screenW * 0.70f;

	const float rankingTitleY = screenH * 0.090f;
	const float rankingFirstRowY = screenH * 0.210f;
	const float rankingRowGap = 108.0f;

	const float currentTimeLabelY = screenH * 0.44f;
	const float currentTimeValueY = screenH * 0.535f;

	// -----------------------------
	// サイズ調整用
	// 今よりほんの少し小さめ
	// -----------------------------
	const int rankingTitleFontSize = 150;      // 旧 168
	const int rankingRowFontSize = 112;        // 旧 126
	const int currentLabelFontSize = 112;      // 旧 126
	const int currentValueFontSize = 150;      // 旧 168

	rankingTitleSprite_ = CreateTextSprite(
		"RANKING",
		"resources/generated_ui/res_ranking_title.png",
		rankingTitleY,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		rankingTitleFontSize
	);

	if (rankingTitleSprite_) {
		rankingTitleSprite_->SetPos({ rankingX, rankingTitleY });
	}

	for (int i = 0; i < kClearTimeRankingMaxCount_; ++i) {
		std::ostringstream oss;
		oss << (i + 1) << ". ";

		if (i < static_cast<int>(clearTimeRanking_.size())) {
			oss << FormatClearTime(clearTimeRanking_[i]);
		}
		else {
			oss << "--:--.--";
		}

		const float rowY = rankingFirstRowY + static_cast<float>(i) * rankingRowGap;

		auto sprite = CreateTextSprite(
			oss.str(),
			"resources/generated_ui/res_ranking_" + std::to_string(i) + ".png",
			rowY,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			rankingRowFontSize
		);

		if (sprite) {
			sprite->SetPos({ rankingX, rowY });

			if (i == clearTimeRankingInsertedIndex_) {
				sprite->SetColor({ 1.0f, 0.9f, 0.15f, 1.0f });
			}
			else {
				sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		rankingTimeSprites_.push_back(std::move(sprite));
	}

	currentTimeLabelSprite_ = CreateTextSprite(
		"YOUR TIME",
		"resources/generated_ui/res_current_time_label.png",
		currentTimeLabelY,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		currentLabelFontSize
	);

	if (currentTimeLabelSprite_) {
		currentTimeLabelSprite_->SetPos({ currentTimeX, currentTimeLabelY });
	}

	currentTimeValueSprite_ = CreateTextSprite(
		FormatClearTime(clearTimeSeconds),
		"resources/generated_ui/res_current_time_value.png",
		currentTimeValueY,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		currentValueFontSize
	);

	if (currentTimeValueSprite_) {
		currentTimeValueSprite_->SetPos({ currentTimeX, currentTimeValueY });
		currentTimeValueSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}
}
