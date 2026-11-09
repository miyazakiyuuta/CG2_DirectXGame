#pragma once
#include <string>
#include <vector>

class EnemySpawner;
struct StageData;

// ステージの進み具合を「区間」ごとに数える係(ゲームシステム: 区間ごとの目標と、クリア時の評価)。
// 区間の区切りはStageData::sections、敵の結末(撃破・取り逃し)はEnemySpawnerのSpawnPointが持っていて、
// ここは毎フレームそれを数え直すだけ。数え直すので、加算のための状態を持たない:
// リトライやPIEのStop/Playで敵の記録が開始時に戻れば、集計も自動的に戻る
// (スコアを撃破数から導いているのと同じ考え方)。
// なぜapplication/stage/に置かないか: 区間ごとの評価はゲーム進行のルールで、
// Stageは「ステージデータからランタイム実体を作る器」に留めたいため(EnemySpawnerと同じ理由)
class StageProgress {
public:
	// 区間1個分の集計
	struct SectionResult {
		std::string name;
		float startDistance = 0.0f; // [m]
		float endDistance = 0.0f;   // [m] 次の区間の開始(最後の区間は終点)
		int total = 0;    // この区間に出る敵の数(出現する進行度がこの区間に入るSpawnPointの数)
		int defeated = 0; // 倒した数
		int missed = 0;   // 取り逃した数(自機の後ろへ抜けた)
		// 区間の敵すべての結末が決まったか(未出現・交戦中が残っていない)。敵のいない区間は確定しない
		bool resolved = false;
		// 取り逃しなしで確定した(区間ごとの目標を達成した)
		bool perfect = false;
	};

	// クリア時の評価(駆除報告)。上ほど良い
	enum class Rank { S, A, B, C };

	/// <summary>
	/// 区間ごとの数を数え直し、前のフレームから新しく確定した区間を調べる。毎フレーム、敵の更新の後に呼ぶ
	/// </summary>
	/// <param name="stageData">区間の区切り(sections)を読む。空ならステージ全体を1区間として扱う</param>
	/// <param name="railLength">レールの全長[m](最後の区間の終わり)</param>
	/// <param name="enemySpawner">敵の結末(SpawnPoint::outcome)を読む</param>
	void Update(const StageData& stageData, float railLength, const EnemySpawner& enemySpawner);

	const std::vector<SectionResult>& GetSections() const { return sections_; }

	// このフレームで新しく確定した区間の添字(区間の結果表示に使う)。無ければ空
	const std::vector<size_t>& GetNewlyResolved() const { return newlyResolved_; }

	// ステージ全体の数
	int GetTotalCount() const;
	int GetDefeatedCount() const;
	// PERFECTで確定した区間の数(スコアのボーナスに使う)
	int GetPerfectCount() const;
	// 駆除率(0〜1)。まだ出ていない敵・交戦中の敵は「倒していない」側に数える。敵が1体もいなければ1
	float GetDefeatRatio() const;

	/// <summary>
	/// クリア時のランク。駆除率と被弾数から決める(基準は"Progress"ウィンドウで調整できる)
	/// </summary>
	/// <param name="damageTaken">このプレイで受けたダメージの合計(最大HP - 残りHP)</param>
	Rank ComputeRank(int damageTaken) const;
	static const char* RankName(Rank rank);

	// "Progress"ウィンドウ: 区間ごとの集計・全体の駆除率・今クリアしたときのランク・ランクの基準(Debug構成のみ)
	void DrawImGui(int damageTaken);

private:
	// ランク1段分の基準。駆除率がminDefeatRatio以上、かつ被弾がmaxDamage以下ならそのランク
	struct RankRule {
		float minDefeatRatio = 0.0f;
		int maxDamage = -1; // -1 = 被弾数を問わない
	};

	// 進行度[m]がどの区間に入るかの添字。先頭の区間より手前も先頭の区間に入れる
	size_t SectionIndexOf(float railDistance) const;

	// 上から順に見て、最初に満たしたランクになる(どれも満たさなければC)
	RankRule rankS_ = { 0.9f, 0 };   // ほぼ全部倒し、無傷
	RankRule rankA_ = { 0.75f, 1 };  // 4分の3以上倒し、被弾1回まで
	RankRule rankB_ = { 0.5f, -1 };  // 半分以上倒す(被弾は問わない)

	std::vector<SectionResult> sections_;
	std::vector<size_t> newlyResolved_;
	// 前のフレームの確定状態(区間ごと)。今フレームと比べて「新しく確定した」区間を見つける。
	// 確定→未確定(リセット)は拾わないので、リセット側でこれを戻す必要はない
	std::vector<bool> previousResolved_;
};
