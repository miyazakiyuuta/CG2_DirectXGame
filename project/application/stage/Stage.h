#pragma once
#include "stage/StageData.h"
#include "scene/EditorObject.h"
#include "math/CatmullRomSpline.h"
#include "math/Collision.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Object3d;

// ステージのランタイム実体。StageDataを元にObject3d群を構築・描画する。
// 「Reload = ファイル読み + Rebuild / (将来の)Play in Editor = Capture + Rebuild」を
// 同一経路にするため、ファイル読み込みとデータからの構築(Rebuild)を分離している。
class Stage {
public:
	/// <summary>
	/// stage.jsonを読み込み、Object3d群を構築する
	/// </summary>
	/// <param name="path">作業ディレクトリ(プロジェクト直下)からの相対パス</param>
	/// <returns>読み込めたら true(失敗時は現在の状態を保つ)</returns>
	bool LoadFromFile(const std::string& path);

	// 保持中のStageDataからObject3d群を作り直す(Reload/Play in Editorの共通経路)
	void Rebuild();

	// --- エディタ用のデータ操作 ---
	// いずれも data_.objects を構造変更して Rebuild() まで行う(実体との整合を崩さない)。
	// 描画コマンド記録後(ImGuiのボタン処理等)に呼んでも安全。Rebuildは古い実体を即deleteせず
	// graveyard_へ退避し、次のUpdate冒頭で破棄する(記録済みコマンドの参照を守る遅延削除)。
	// 呼び出し側は BuildEditorObjects() の一覧を必ず作り直すこと(Transformポインタが無効になる)

	/// <summary>
	/// オブジェクトを末尾に追加する。nameは既存と重複しないよう採番される
	/// </summary>
	/// <returns>追加したオブジェクトの添字</returns>
	size_t AddObject(StageData::ObjectData objectData);

	/// <summary>
	/// index番のオブジェクトを複製し、Hierarchy上で隣に並ぶよう直後へ挿入する(nameは採番)
	/// </summary>
	/// <returns>複製したオブジェクトの添字(index+1)</returns>
	size_t DuplicateObject(size_t index);

	/// <summary>
	/// index番のオブジェクトを、X・Z方向へ格子状に複製する(床の板のように、同じ物を一定間隔で敷き詰める用)。
	/// 元のオブジェクトを(-X,-Z)側の角として、+X・+Z方向へ countX×countZ 個になるよう並べる(元の1個を含む)。
	/// 回転は元のまま(床の板は回さず同じ向きで敷く前提)。複製はHierarchy上で元の直後にまとめて並ぶ
	/// </summary>
	/// <returns>複製した個数(countX×countZ-1)</returns>
	size_t DuplicateObjectGrid(size_t index, int countX, int countZ, float spacingX, float spacingZ);

	/// <summary>
	/// index番のオブジェクトを削除する
	/// </summary>
	void RemoveObject(size_t index);

	/// <summary>
	/// index番のオブジェクトのモデルを差し替える。構造変更(要素の追加/削除)ではないため、
	/// 上記と異なりBuildEditorObjects()の一覧は作り直さなくてよい(Transformポインタは有効なまま)
	/// </summary>
	void SetObjectModel(size_t index, const std::string& model);

	/// <summary>
	/// index番のオブジェクトの無効フラグを切り替える(trueで実体を破棄、falseで再生成)。
	/// SetObjectModelと同じく構造変更ではないため、一覧の作り直しは不要。
	/// ただしEditorObjectのdisabled/pickableの表示状態は呼び出し側が更新すること
	/// </summary>
	void SetObjectDisabled(size_t index, bool disabled);

	// --- レール(制御点)の編集 ---
	// 制御点を動かす・足す・消すとレールの長さが変わる。そのとき、レールに沿った距離で書かれているデータ
	// (区間の開始距離・敵の出現距離railDistance)は、数値のままだと「レール上の別の場所」を指してしまう。
	// そこで形を変えるたびに、それらを「何番目の区間の何割の位置か」に置き換えてから新しい曲線の距離へ戻し、
	// レール上の同じ場所に貼り付いたまま付いてくるようにする(敵の出現位置・女王の位置はワールド座標なので動かない)

	/// <summary>
	/// Hierarchy/Inspector/ギズモ用の制御点の一覧を作る。positionはdata_.railの要素を直接指す
	/// (配置オブジェクトがdata_.objectsのtransformを直接指すのと同じ)。
	/// 制御点の数を変えたら(Insert/Remove/Reload)必ず作り直すこと(ポインタが無効になる)
	/// </summary>
	std::vector<EditorObject> BuildRailEditorObjects();

	/// <summary>
	/// index番の制御点の後ろに制御点を1つ挿入する。位置は次の点との間の、今の曲線上の中間
	/// (曲線の上に置くので形がほとんど変わらない)。最後の点の後ろなら、直前の区間と同じ向き・長さだけ先へ延ばす
	/// </summary>
	/// <returns>挿入した制御点の添字(index+1)</returns>
	size_t InsertRailPoint(size_t index);

	/// <summary>
	/// index番の制御点を消す。曲線が作れなくなるので、制御点が2個以下のときは何もしない
	/// </summary>
	void RemoveRailPoint(size_t index);

	// レールを作り直した通算回数。シーンが前回見た値と比べ、変わっていたら
	// レールに沿った距離を写して持っている側(EnemySpawner)を作り直す(「差分を見る」作法)
	uint32_t GetRailRevision() const { return railRevision_; }

	// 点光源のうち、シェーダの上限(kMaxPointLights)を超えて使われなかった数。エディタの警告用
	uint32_t GetPointLightOverflow() const { return pointLightOverflow_; }

	void Update(float deltaTime);
	void Draw();

	// 描画に使うカメラを差し替える(構築済みの全オブジェクトへも反映)
	void SetCamera(Camera* camera);

	/// <summary>
	/// Hierarchy/Inspector/ギズモ用の一覧を作る。
	/// transformはdata_.objectsの要素を直接指す(StageDataが唯一の編集対象)ため、
	/// data_.objectsを構造変更(追加/削除/Reload)したら必ず作り直すこと
	/// </summary>
	std::vector<EditorObject> BuildEditorObjects();

	StageData& GetData() { return data_; }
	const StageData& GetData() const { return data_; }

	// レール曲線。距離→位置/接線の評価はCatmullRomSpline自身が持つ(レールカメラ等はこれを直接使う)。
	// data_.railを書き換えたらRebuild()を呼ぶまで反映されない点に注意
	const CatmullRomSpline& GetRail() const { return rail_; }

	/// <summary>
	/// 当たり判定用のワールド空間AABB一覧(コライダー設定のあるオブジェクトのみ)。
	/// エディタでの移動に追従させるためUpdateで毎フレーム作り直す。
	/// 無効フラグ(disabled)のオブジェクトは実体と同様に除外される
	/// </summary>
	const std::vector<AABB>& GetWorldColliders() const { return worldColliders_; }

	Stage();
	~Stage();

private:
	// baseが既存オブジェクト名と重複する場合、"base_1" "base_2" と加算して空き名を返す
	std::string MakeUniqueName(const std::string& base) const;

	// data_.objectsのcollider(オブジェクト中心からのオフセット)とTransformから
	// worldColliders_を作り直す。Rebuild(構造変更)とUpdate(ライブ編集)の両方から呼ぶ
	void UpdateWorldColliders();

	/// <summary>
	/// ステージの光(lighting の環境光・月明かりと、objects[]の点光源)をエンジンの共通の光(Object3dCommon)へ渡す。
	/// 毎フレームUpdateから呼ぶので、エディタでの編集がその場で効く。lighting項目が無ければ何もしない。
	/// ドローンのライト(スポットライト)は自機の位置と照準で決まるので、ここではなくシーンが渡す
	/// </summary>
	void ApplyLights();

	/// <summary>
	/// data_.railの今の制御点でレールを作り直し、レールに沿った距離のデータ(区間の開始・敵の出現距離)を付け直す。
	/// 手順: ①今の曲線(rail_。まだ古い形)で各距離を「何番目の区間(制御点 i と i+1 の間)の、長さの何割か」に置き換える
	/// ②remapSegmentで区間番号を付け替える(挿入・削除で番号がずれるため。動かしただけなら何もしない)
	/// ③新しい制御点で曲線を作り直し、距離へ戻す。
	/// 区間番号と割合で覚えるのは、その区間より手前の長さが変わっても同じ場所を指し続けるから。
	/// 定義はStage.cppだけで使うのでそちらに置く(区間番号と割合の型RailAnchorも.cpp内)
	/// </summary>
	template <class RemapSegment>
	void RebuildRailKeepingDistances(RemapSegment remapSegment);

	StageData data_;
	// data_.railから構築したレール曲線(Rebuildで再構築される)
	CatmullRomSpline rail_;
	// レールを作り直した通算回数(GetRailRevision)
	uint32_t railRevision_ = 0;
	// 上限を超えて使われなかった点光源の数(GetPointLightOverflow)
	uint32_t pointLightOverflow_ = 0;
	// 当たり判定用のワールドAABB。data_.objectsとは要素数が異なる(コライダー付きのみ)
	std::vector<AABB> worldColliders_;
	Camera* camera_ = nullptr;
	// data_.objectsと同じ並びのランタイム実体(disabledの要素はnullptr。indexで対応が取れる)
	std::vector<std::unique_ptr<Object3d>> objects_;
	// Rebuildで置き換えられた古い実体の待機場所。フレーム内で記録済みのコマンドが
	// まだ参照している可能性があるため、次のUpdate冒頭(前フレームのGPU実行完了後)まで生かす
	std::vector<std::unique_ptr<Object3d>> graveyard_;
};
