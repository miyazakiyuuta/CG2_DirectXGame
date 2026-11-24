#pragma once
#include "scene/BaseScene.h"
#include "math/Vector2.h"
#include "math/Vector4.h"

#include <array>
#include <memory>
#include <string>

class NumberSprite;
class Sprite;
struct SoundData;

// 結果表示(A-5)の基底クラス。クリアとゲームオーバーで共通の部分(結果の文言・スコア・RETRY/TITLEの選択肢・入力)を持ち、
// 違う部分(文言の画像と色・結果音・縦の並び)だけを純粋仮想関数として派生クラスに任せる。
//   ResultScene(抽象)
//    ├ ClearResultScene    … 白い文言・クリア音・ランク(駆除報告)の段あり
//    └ GameOverResultScene … 赤みの文言・ゲームオーバー音
// 以前は1クラスで両方を兼ね、enum Typeとifで出し分けていた。種類を値で持って各所で分岐する作りは、
// 種類が増えるたびに分岐が増えていくので、種類ごとの派生クラスに置き換えた(SceneFactoryがシーン名で作り分ける)。
// ランクはクリアにしか無いので、基底ではなくClearResultSceneに置く
// (基底に置くと、ゲームオーバーが「何もしないランク表示」を持たされる。飛べない鳥にFlyを持たせるのと同じ誤り)。
//
// 最後に「RETRY / TITLE」を選ばせる。RETRYはGamePlaySceneを作り直す(=HP・敵・弾・進行度がすべて初期化される)、
// TITLEはタイトルへ戻る。どちらも結果を読んでから自分で選ぶので、「唐突にタイトルへ飛ばされる」ことがない。
class ResultScene : public BaseScene {
public:
	/// <summary>
	/// 結果画面に表示するスコアを渡す。GamePlaySceneが遷移を要求する直前に呼ぶ。
	/// なぜstaticか: SceneManager::ChangeSceneはシーン名しか渡せず、実体は
	/// SceneFactoryが後から作るため、コンストラクタ引数では渡せない。
	/// ResultSceneが自分の入力を自分で預かる形にして、SceneManager/SceneFactoryには手を入れない
	/// </summary>
	static void SetScore(int score);

	// 共通部分を作る。派生クラスが足す部分(ランクなど)は、派生クラスがこれを呼んだ後に作る
	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	// 基底のポインタ(unique_ptr<BaseScene>)で破棄されるので、BaseSceneの仮想デストラクタを引き継ぐ
	~ResultScene() override;

protected:
	// 定義は.cpp側。ヘッダに書くとunique_ptr<Sprite>の破棄が
	// 前方宣言だけのSpriteを要求してしまう(コンストラクタの例外経路でデストラクタが要る)。
	// protectedにするのは、結果の種類を決めずに作れない(派生クラスからしか作らせない)ため
	ResultScene();

	// 縦の並び(画面の高さに対する割合)。ランクの段の有無で、クリアとゲームオーバーで変わる
	struct Layout {
		float resultY = 0.42f;  // 結果の文言の中心
		float scoreTop = 0.55f; // スコアの上端
		float menuTopY = 0.70f; // 選択肢の上段(RETRY)の中心。下段はこの下に並べる
	};

	// --- 派生クラスが決める違い ---
	// 結果の文言の画像(エンジンにフォント描画が無いため、文言は画像として持つ)
	virtual std::string GetResultTexturePath() const = 0;
	// 文言の色(一目で結果を区別できるようにする)
	virtual Vector4 GetResultColor() const = 0;
	// 表示と同時に1回鳴らす結果音
	virtual std::string GetResultSoundPath() const = 0;
	virtual Layout GetLayout() const = 0;

	// 画像は1280x720想定で作ってあるので、実解像度に合わせて拡大する倍率
	// (kClientWidthは構成で変わる(Debug=1920x1080 / Release=1280x720)ため直値は使えない)
	static float GetUiScale();

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

	// 結果の文言(clear.png / gameover.png)
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
