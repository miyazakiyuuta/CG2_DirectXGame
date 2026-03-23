#pragma once
#define NOMINMAX

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "3d/Camera.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "base/WinApp.h"
#include "debug/DebugSphere.h"
#include "io/Input.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "utility/CollisionUtility.h"
#include <../externals/nlohmann/json.hpp>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <unordered_map>


#include "StageTypes.h"
#include "StageSerializer.h"
#include "StageLoader.h"
#include "EditorController.h"

/// <summary>
/// ステージエディタクラス
/// </summary>
class StageEditor {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    StageEditor(Object3dCommon* objCommon, Camera* camera);

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(const std::string& defaultModel);

    /// <summary>
    /// 更新処理（入力チェックとオブジェクト生成・削除）
    /// </summary>
    void Update();

    /// <summary>
    /// 描画処理（StageLoaderの描画呼び出し）
    /// </summary>
    void Draw();

    /// <summary>
    /// 編集モードの切り替え
    /// </summary>
    void ToggleEditMode();

    /// <summary>
    /// 編集モードかどうかを返す
    /// </summary>
    bool IsEditMode() const;

    /// <summary>
    /// JSONファイルにステージデータを保存
    /// </summary>
    bool Save(const std::string& path);

    /// <summary>
    /// JSONファイルからステージデータを読み込み、オブジェクトを生成
    /// </summary>
    bool Load(const std::string& path);

    /// <summary>
    /// ステージデータとオブジェクトをすべてクリア
    /// </summary>
    void Clear();

    /// <summary>
    /// ステージデータへの参照を返す（外部から直接データを操作したい場合に使用）
    /// </summary>
    StageData& GetStageData();

    /// <summary>
    /// ブロック用の AABB 一覧を返す
    /// </summary>
    std::vector<CollisionUtility::AABB> GetBlockAABBs() const;

    /// <summary>
    /// 水ブロック用の AABB 一覧を返す
    /// </summary>
    std::vector<CollisionUtility::AABB> GetWaterBlockAABBs() const;

    /// <summary>
    /// 虫スポーン位置一覧を返す
    /// </summary>
    std::vector<Vector3> GetBugSpawnPositions() const;

    /// <summary>
    /// プレイヤー開始位置を返す（存在しない場合は std::nullopt）
    /// </summary>
    std::optional<Vector3> GetPlayerSpawnPosition() const;

private:
    // スナップショット構造体（履歴保存用）
    struct Snapshot {
        StageData data;
        int nextId = 1;
    };

    // 履歴（線形履歴）と現在の履歴インデックス
    std::vector<Snapshot> history_;
    int historyIndex_ = -1;

    /// <summary>
    /// 現在のステージデータのスナップショットを履歴に保存する関数
    /// </summary>
    void SaveHistorySnapshot();

    /// <summary>
    /// 生成基準点を取得する関数
    /// </summary>
    Vector3 GetCreateOrigin() const;

    /// <summary>
    /// Undo / Redo
    /// </summary>
    void Undo();
    void Redo();

    /// <summary>
    /// ImGui を使用して編集モードの UI を描画する関数
    /// </summary>
    void DrawImGui();

    /// <summary>
    /// UI 表示用: ブロック種別に応じた表示モデル名を返す
    /// </summary>
    std::string ResolveDisplayModelName(const StageObject& o) const;

    /// <summary>
    /// UI 表示用: ブロック種別に応じた表示拡縮を返す
    /// </summary>
    Vector3 ResolveDisplayScale(const StageObject& o) const;

    // 保存データ本体
    StageData data_;

    // 描画インスタンス管理
    StageLoader loader_;

    // 入力処理
    EditorController controller_;

    // Object3d 初期化用
    Object3dCommon* object3dCommon_ = nullptr;

    // 描画時に使用するカメラ
    Camera* camera_ = nullptr;

    // ------------------------------------------------------------------
    // 以下はエディタの状態
    // ------------------------------------------------------------------

    // 新規生成オブジェクトのデフォルトモデル名
    std::string defaultModel_ = "Cube.obj";
    // ImGui 用固定長バッファ: std::string の内部バッファを直接渡さない安全な実装
    char defaultModelBuf_[256] = {};
    // バッチ生成用モデルバッファ
    char batchModelBuf_[256] = {};

    // バッチ生成設定
    int batchCountA_ = 3; // 横方向の数
    int batchCountB_ = 3; // 縦方向の数
    float batchSpacing_ = 2.0f; // 間隔
    int batchNormalIndex_ = 1; // 0=X,1=Y(floor),2=Z

    // 次に付与するID
    int nextId_ = 1;

    // カスタム生成原点と参照選択
    Vector3 createOrigin_ { 0.0f, 0.0f, 5.0f };
    int createReferenceIndex_ = 1; // 0=Camera Forward,1=World Origin,2=Selected,3=Custom
    Vector3 createOffset_ { 0.0f, 0.0f, 0.0f };

    // 編集モード状態
    bool isEditMode_ = false;
    // リアルタイム編集フラグ（Apply 押下不要で即時反映）
    bool liveEdit_ = true;

    // 選択中のオブジェクトID
    int selectedObjectId_ = -1;
    // 選択中オブジェクトのモデル編集用バッファ
    char selectedModelBuf_[256] = {};

    // 選択中オブジェクトの編集用ワークバッファ
    Vector3 editPosition_ {};
    Vector3 editRotation_ {};
    Vector3 editScale_ { 1.0f, 1.0f, 1.0f };
    Vector4 editColor_ { 1.0f, 1.0f, 1.0f, 1.0f };

    // 選択ハイライトの点滅制御
    std::chrono::steady_clock::time_point selectionLastBlinkTime_;
    float selectionBlinkInterval_ = 3.0f; // 秒
    bool selectionBlinkOn_ = true;

    // プレビュー表示用
    std::unique_ptr<Object3d> previewMarker_;
    std::unique_ptr<DebugSphere> previewSphere_;
    bool showCreatePreview_ = true;
    bool showBatchPreview_ = false;
    float previewRadius_ = 1.0f;

    // 原点移動モード
    bool moveOriginMode_ = false;
    float moveSensitivity_ = 0.02f;
    float moveDepthSensitivity_ = 0.1f;
    float createHeightSensitivity_ = 0.5f;

    // ホットキーで原点移動モードをトグル
    bool useHotkey_ = false;
    char toggleKeyBuf_[16] = "M";
    bool hotkeyPrevDown_ = false;

    // 新規生成時の設定
    BlockID placingBlockId_ = BlockID::Normal;
    Vector4 placingColor_ { 1.0f, 1.0f, 1.0f, 1.0f };

    // 選択表示用ハイライトカラー
    Vector4 selectionHighlightColor_ { 1.0f, 1.0f, 1.0f, 1.0f };
    float selectionBlinkAlpha_ = 0.5f;

    // 編集用ブロック種別
    BlockID editBlockId_ = BlockID::Normal;

    // 複製オプション
    int duplicateCount_ = 1;
    Vector3 duplicateOffset_ = { 2.0f, 0.0f, 0.0f };
    bool useHalfSizeOffset_ = true;
};
