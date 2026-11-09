#include "progress/StageProgress.h"

#include "enemy/EnemySpawner.h"
#include "stage/StageData.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void StageProgress::Update(const StageData& stageData, float railLength, const EnemySpawner& enemySpawner) {
	// 区切りを写す。区間が無いステージは全体を1区間として扱う
	// (sectionsは「開始距離の昇順・先頭は0」に揃っている: StageData::sectionsの約束)
	sections_.clear();
	if (stageData.sections.empty()) {
		SectionResult whole;
		whole.name = "Stage";
		whole.endDistance = railLength;
		sections_.push_back(std::move(whole));
	} else {
		for (size_t i = 0; i < stageData.sections.size(); ++i) {
			SectionResult section;
			section.name = stageData.sections[i].name;
			section.startDistance = stageData.sections[i].startDistance;
			section.endDistance = i + 1 < stageData.sections.size()
				? stageData.sections[i + 1].startDistance
				: railLength;
			sections_.push_back(std::move(section));
		}
	}

	// 敵を区間へ振り分けて数える。所属は「出現する進行度(railDistance)がどの区間の範囲に入るか」。
	// 区間の側に敵の一覧を持たせないので、1/6で敵を入れ替えても区間のデータは触らずに済む
	using Outcome = EnemySpawner::SpawnPoint::Outcome;
	for (const EnemySpawner::SpawnPoint& spawnPoint : enemySpawner.GetSpawnPoints()) {
		SectionResult& section = sections_[SectionIndexOf(spawnPoint.railDistance)];
		++section.total;
		if (spawnPoint.outcome == Outcome::Defeated) {
			++section.defeated;
		} else if (spawnPoint.outcome == Outcome::Missed) {
			++section.missed;
		}
	}
	for (SectionResult& section : sections_) {
		section.resolved = section.total > 0 && section.defeated + section.missed == section.total;
		section.perfect = section.resolved && section.missed == 0;
	}

	// 前のフレームから新しく確定した区間を探す(被弾をHPの前後で拾うのと同じ「差分を見る」やり方)。
	// 区間の数が変わったフレーム(エディタで追加・削除した)は比べられないので、何も確定させない
	newlyResolved_.clear();
	if (previousResolved_.size() == sections_.size()) {
		for (size_t i = 0; i < sections_.size(); ++i) {
			if (sections_[i].resolved && !previousResolved_[i]) {
				newlyResolved_.push_back(i);
			}
		}
	}
	previousResolved_.resize(sections_.size());
	for (size_t i = 0; i < sections_.size(); ++i) {
		previousResolved_[i] = sections_[i].resolved;
	}
}

size_t StageProgress::SectionIndexOf(float railDistance) const {
	// 開始距離がrailDistanceを超える最初の区間を探し、その1つ前が所属先
	const auto next = std::upper_bound(sections_.begin(), sections_.end(), railDistance,
		[](float distance, const SectionResult& section) { return distance < section.startDistance; });
	return next == sections_.begin() ? 0 : static_cast<size_t>(next - sections_.begin()) - 1;
}

int StageProgress::GetTotalCount() const {
	int count = 0;
	for (const SectionResult& section : sections_) {
		count += section.total;
	}
	return count;
}

int StageProgress::GetDefeatedCount() const {
	int count = 0;
	for (const SectionResult& section : sections_) {
		count += section.defeated;
	}
	return count;
}

int StageProgress::GetPerfectCount() const {
	return static_cast<int>(std::count_if(sections_.begin(), sections_.end(),
		[](const SectionResult& section) { return section.perfect; }));
}

float StageProgress::GetDefeatRatio() const {
	const int total = GetTotalCount();
	return total > 0 ? static_cast<float>(GetDefeatedCount()) / static_cast<float>(total) : 1.0f;
}

StageProgress::Rank StageProgress::ComputeRank(int damageTaken) const {
	const float ratio = GetDefeatRatio();
	auto satisfies = [&](const RankRule& rule) {
		return ratio >= rule.minDefeatRatio && (rule.maxDamage < 0 || damageTaken <= rule.maxDamage);
	};
	if (satisfies(rankS_)) {
		return Rank::S;
	}
	if (satisfies(rankA_)) {
		return Rank::A;
	}
	if (satisfies(rankB_)) {
		return Rank::B;
	}
	return Rank::C;
}

const char* StageProgress::RankName(Rank rank) {
	switch (rank) {
	case Rank::S: return "S";
	case Rank::A: return "A";
	case Rank::B: return "B";
	default:      return "C";
	}
}

void StageProgress::DrawImGui(int damageTaken) {
#ifdef USE_IMGUI
	ImGui::Begin("Progress");

	// 区間ごとの集計。Statusは 交戦中(...) / 確定(取り逃しあり) / PERFECT
	if (ImGui::BeginTable("##progress", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Section");
		ImGui::TableSetupColumn("Range");
		ImGui::TableSetupColumn("Defeated");
		ImGui::TableSetupColumn("Missed");
		ImGui::TableSetupColumn("Status");
		ImGui::TableHeadersRow();
		for (const SectionResult& section : sections_) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(section.name.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.0f - %.0f m", section.startDistance, section.endDistance);
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%d / %d", section.defeated, section.total);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%d", section.missed);
			ImGui::TableSetColumnIndex(4);
			if (section.total == 0) {
				ImGui::TextDisabled("(no enemies)");
			} else if (section.perfect) {
				ImGui::TextColored({ 1.0f, 0.85f, 0.2f, 1.0f }, "PERFECT");
			} else if (section.resolved) {
				ImGui::Text("Done");
			} else {
				ImGui::TextDisabled("...");
			}
		}
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Total");
	ImGui::Text("Defeated: %d / %d (%.0f%%)", GetDefeatedCount(), GetTotalCount(), GetDefeatRatio() * 100.0f);
	ImGui::Text("Perfect Sections: %d / %zu", GetPerfectCount(), sections_.size());
	ImGui::Text("Damage Taken: %d", damageTaken);
	ImGui::Text("Rank if cleared now: %s", RankName(ComputeRank(damageTaken)));

	// ランクの基準。上から順に見て、最初に満たしたランクになる
	ImGui::SeparatorText("Rank Rules (Max Damage -1 = any)");
	auto drawRule = [](const char* label, RankRule& rule) {
		ImGui::PushID(label);
		ImGui::Text("%s", label);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Min Defeat", &rule.minDefeatRatio, 0.0f, 1.0f, "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragInt("Max Damage", &rule.maxDamage, 0.1f, -1, 10);
		ImGui::PopID();
	};
	drawRule("S", rankS_);
	drawRule("A", rankA_);
	drawRule("B", rankB_);

	ImGui::End();
#else
	(void)damageTaken;
#endif
}
