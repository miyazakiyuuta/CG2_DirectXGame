#pragma once
#include "scene/BaseScene.h"
#include "progress/StageProgress.h"
#include "math/Vector2.h"

#include <array>
#include <memory>
#include <string>

class NumberSprite;
class Sprite;
struct SoundData;

// クリア/ゲームオーバーの結果表示(A-5)。
// 1クラスで両方を兼ね、どちらを出すかは生成時のTypeで決める。
// なぜTypeを引数に取るか: SceneManager::ChangeSceneはシーン名しか渡せないため、
// SceneFactoryが "CLEAR"/"GAMEOVER" を別のTypeで作り分ける。
// こうするとエンジン層(SceneManager)に「結果の種別」という概念を持ち込まずに済む。
// 最後に「RETRY / TITLE」を選ばせる。RETRYはGamePlaySceneを作り直す(=HP・敵・弾・進行度がすべて初期化される)、
// TITLEはタイトルへ戻る。どちらも結果を読んでから自分で選ぶので、「唐突にタイトルへ飛ばされる」ことがない。
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

	/// <summary>
	/// RETRYを選んだときに移るシーン名を渡す(本編は"GAMEPLAY"、デモは"CLEAR_DEMO")。
	/// SetScoreと同じく、GamePlaySceneが遷移を要求する直前に毎回呼ぶ
	/// </summary>
	static void SetRetryScene(const std::string& sceneName); // CLEAR_DEMO

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	~ResultScene() override;

private:
	// 選択肢。並びは画面の上から順(最初に選ばれているのは先頭=RETRY)
	enum class MenuItem {
		Retry, // もう一度遊ぶ
		Title, // タイトルへ戻る
		Count,
	};
	static constexpr int kMenuCount = static_cast<int>(MenuItem::Count);

	// 選択肢を上/下へ動かす入力が入ったか(↑↓・W/S・十字キー・左スティック)
	bool IsUpPressed() const;
	bool IsDownPressed() const;
	// 決定の入力が入ったか(Enter/Space/パッドA/START)
	bool IsDecidePressed() const;

	// 選んでいる選択肢へ遷移する
	void Decide();

	// 選択肢の見た目(選んでいる方は白く大きく脈打たせ、選んでいない方は小さく暗くする)
	void UpdateMenu();

	Type type_ = Type::GameOver;

	// 結果の文言(clear.png / gameover.png)。
	// エンジンにフォント描画が無いため、文言は画像として持つ
	std::unique_ptr<Sprite> resultSprite_;

	// 選択肢(menu_retry.png / menu_title.png)。並びはMenuItemと同じ
	std::array<std::unique_ptr<Sprite>, kMenuCount> menuSprites_;
	// 選んでいるときの表示サイズ[px]。選んでいない方はこれを縮めて出すので、元の大きさを控えておく
	std::array<Vector2, kMenuCount> menuSizes_{};
	int selected_ = 0; // 選んでいる選択肢(MenuItemの番号)
	bool decided_ = false; // 決定済みか(遷移の帯が閉じる間に、選び直したり二重に決定したりしない)
	// 左スティックの縦の傾き(前のフレーム)。倒した瞬間だけ1つ動かすため、前の値と比べる
	float previousStickY_ = 0.0f;
	float stickY_ = 0.0f;

	// 結果音(クリア/ゲームオーバーで鳴らし分ける)。
	// BGMはGamePlayScene::Finalizeで止まっているので被らない
	std::shared_ptr<const SoundData> resultSound_;

	// スコア表示(数字画像から桁を切り出して並べる)
	std::unique_ptr<NumberSprite> scoreNumber_;

	// ランク(CLEARのときだけ)。「RANK」の見出しと文字1つ。
	// 文字はS/A/B/Cを等幅に並べた1枚の画像から切り出す(数字画像と同じやり方)
	std::unique_ptr<Sprite> rankLabelSprite_;
	std::unique_ptr<Sprite> rankSprite_;

	// 選択肢を出してからの経過[秒](出始めのフェードに使う)。
	// 選択肢を出すのは「入力の猶予が過ぎ、かつ遷移の帯が開き切った」後なので、表示開始からの経過とは別に数える
	float menuTimer_ = 0.0f;

	// 入力を受け付けるまでの猶予[秒]と表示開始からの経過。
	// なぜ猶予が要るか: 射撃(Space/パッドA)を押しっぱなしで死ぬと、
	// 結果表示が出た瞬間に決定してしまい、何が起きたか読めないため。選択肢はこの猶予が過ぎてから出す。
	// 猶予に加えて、遷移の帯が開き切るまでも待つ(遷移中のChangeSceneはSceneManagerに無視されるため)
	float inputLockDuration_ = 0.5f;
	float elapsed_ = 0.0f;
};
