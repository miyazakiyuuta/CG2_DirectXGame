#pragma once
#include "scene/BaseScene.h"
#include "progress/StageProgress.h"

#include <memory>

class NumberSprite;
class Sprite;
struct SoundData;

// クリア/ゲームオーバーの結果表示(A-5)。
// 1クラスで両方を兼ね、どちらを出すかは生成時のTypeで決める。
// なぜTypeを引数に取るか: SceneManager::ChangeSceneはシーン名しか渡せないため、
// SceneFactoryが "CLEAR"/"GAMEOVER" を別のTypeで作り分ける。
// こうするとエンジン層(SceneManager)に「結果の種別」という概念を持ち込まずに済む。
// リトライ入力を受けたらGamePlaySceneを作り直す(=HP・敵・弾・進行度がすべて初期化される)。
class ResultScene : public BaseScene {
public:
	enum class Type {
		Clear,    // レール終端に到達した
		GameOver, // HPが尽きた
	};

	// 定義は.cpp側。ヘッダに書くとunique_ptr<Sprite>の破棄が
	// 前方宣言だけのSpriteを要求してしまう(コンストラクタの例外経路でデストラクタが要る)
	explicit ResultScene(Type type);

	/// <summary>
	/// 結果画面に表示するスコアを渡す。GamePlaySceneが遷移を要求する直前に呼ぶ。
	/// なぜstaticか: SceneManager::ChangeSceneはシーン名しか渡せず、実体は
	/// SceneFactoryが後から作るため、コンストラクタ引数では渡せない。
	/// ResultSceneが自分の入力を自分で預かる形にして、SceneManager/SceneFactoryには手を入れない
	/// (シーン名でTypeを分けているのと同じ方針)
	/// </summary>
	static void SetScore(int score);

	/// <summary>
	/// 結果画面に表示するランク(駆除報告)を渡す。CLEARのときだけ表示する。
	/// SetScoreと同じく、GamePlaySceneが遷移を要求する直前に呼ぶ
	/// (ランクの決め方はStageProgressが持ち、ここは受け取って見せるだけ)
	/// </summary>
	static void SetRank(StageProgress::Rank rank);

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	~ResultScene() override;

private:
	// リトライ入力が入ったか(Enter/Space/パッドA/START)
	bool IsRetryPressed() const;

	Type type_ = Type::GameOver;

	// 結果の文言(clear.png / gameover.png)と操作案内(retry.png)。
	// エンジンにフォント描画が無いため、文言は画像として持つ
	std::unique_ptr<Sprite> resultSprite_;
	std::unique_ptr<Sprite> retrySprite_;

	// 結果音(クリア/ゲームオーバーで鳴らし分ける)。
	// BGMはGamePlayScene::Finalizeで止まっているので被らない
	std::shared_ptr<const SoundData> resultSound_;

	// スコア表示(数字画像から桁を切り出して並べる)
	std::unique_ptr<NumberSprite> scoreNumber_;

	// ランク(CLEARのときだけ)。「RANK」の見出しと文字1つ。
	// 文字はS/A/B/Cを等幅に並べた1枚の画像から切り出す(数字画像と同じやり方)
	std::unique_ptr<Sprite> rankLabelSprite_;
	std::unique_ptr<Sprite> rankSprite_;

	// 入力を受け付けるまでの猶予[秒]と表示開始からの経過。
	// なぜ猶予が要るか: 射撃(Space/パッドA)を押しっぱなしで死ぬと、
	// 結果表示が出た瞬間にリトライしてしまい、何が起きたか読めないため
	float inputLockDuration_ = 0.5f;
	float elapsed_ = 0.0f;
};
