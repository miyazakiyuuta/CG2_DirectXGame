#pragma once

class Camera;

/// <summary>
/// 演出(シーンの進行を一時的に預かる見せ場)のインターフェース。
/// ゲーム開始のイントロ・ゲームオーバー・クリアの3つが実装し、シーンはこの型のポインタでまとめて扱う
/// (時間倍率の掛け合わせ・スキップ・停止・カメラの差し替え・描画・ImGuiを、演出の種類を問わず同じ1行で回す)。
///
/// 継承の形:
///   ISequence
///    ├ IntroCamera       … カメラが回り込むだけの演出(描く物も時間倍率も持たない)
///    └ PhasedSequence    … 「段階」で進む演出の骨組み(Template Method)
///         ├ GameOverSequence
///         └ ClearSequence
///
/// 基底に置かないもの:
///  - 開始(Start): 始めるのに要る情報が演出ごとに違う(被弾した機体の姿勢 / 女王の亡骸 / 何も要らない)。
///    無理に1つの引数へまとめると、どの演出にも要らない引数を渡すことになるので、具象クラスで呼ぶ
///  - 演出に固有の問い合わせ(カメラの見送り・小爆発の回数・段階): 共通の機能ではないので具象クラスに残す
/// </summary>
class ISequence {
public:
	// 基底のポインタで扱うので仮想デストラクタにする(シーンは具象の型で所有しているが、作法としてそろえる)
	virtual ~ISequence() = default;

	/// <summary>
	/// 演出を進める
	/// </summary>
	/// <param name="deltaTime">実時間[秒](ヒットストップやスローの影響を受けない時間)</param>
	/// <param name="camera">描画に使っているカメラ(画面上の位置を求める演出が使う)</param>
	virtual void Update(float deltaTime, const Camera& camera) = 0;

	// 演出が持つ物体を描く。既定では何もしない(カメラだけの演出には描く物が無い)。
	// 「何も描かない」は嘘ではない正しい振る舞いなので、空の既定実装を置いてよい
	// (飛べない鳥に何もしないFlyを持たせるのとは違い、描く物が無ければ描かないのが「描く」の意味どおり)
	virtual void Draw() {}

	// 再生していない状態へ戻す(ゲーム状態のリセットとシーンの終了で使う)。
	// 時間倍率・粒・ポストエフェクトなど全シーン共有の状態を触った演出は、ここで必ず元に戻す
	virtual void Stop() = 0;

	// スキップ入力(Enter/パッドSTART)を受けたとき。どこまで飛ばすか(止める・山場へ進める)は演出ごとに決める
	virtual void Skip() = 0;

	// 描画に使うカメラを差し替える(デバッグカメラの切り替え)。既定では何もしない(描く物が無い演出)
	virtual void SetCamera(Camera* camera) { (void)camera; }

	// 再生中か。シーンはこの間、レールと操作を止める
	virtual bool IsActive() const = 0;

	// ゲーム時間の倍率。シーンは自機・敵・弾に渡すdeltaTimeにこれを掛ける。
	// 既定は等速(時間を操作しない演出)。演出は同時に1つしか起きないので、シーンは全演出の倍率を掛け合わせてよい
	virtual float GetTimeScale() const { return 1.0f; }

	// 進み具合と調整値をImGuiに出す(Debug構成のみ中身がある)
	virtual void DrawImGui() {}
};
