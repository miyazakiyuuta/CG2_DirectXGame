#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "StageTypes.h"
#include "3d/Object3dCommon.h"
#include "3d/Object3d.h"
#include "3d/Camera.h"

/// <summary>
/// StageData の内容を元に Object3d インスタンスを生成・管理するクラス
/// </summary>
class StageLoader {
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    StageLoader(Object3dCommon* objCommon, Camera* camera);

    /// <summary>
    /// StageData の内容を元に Object3d インスタンスを生成
    /// (差分更新を行う)
    /// </summary>
    void CreateFromData(const StageData& data);

    /// <summary>
    /// StageObject の内容を元に Object3d インスタンスを更新・作成する
    /// </summary>
    void UpdateOrCreateInstance(const StageObject& o);

    /// <summary>
    /// ID でインスタンスを照合して削除
    /// </summary>
    void RemoveInstanceById(int id);

    /// <summary>
    /// ID でインスタンスを照合して位置・回転・拡縮率を更新
    /// </summary>
    void UpdateInstanceTransform(int id, const Vector3& pos, const Vector3& rot, const Vector3& scale);

    /// <summary>
    /// インスタンスに直接カラーを設定する（選択ハイライト用）
    /// </summary>
    void SetInstanceColorById(int id, const Vector4& color);

    /// <summary>
    /// すべてのインスタンスを破棄
    /// </summary>
    void Clear();

    /// <summary>
    /// 管理している全インスタンスの Update と Draw を呼び出す
    /// </summary>
    void DrawAndUpdate();

    // Debug: インスタンス数を返す
    size_t GetInstanceCount() const;

private:
    /// <summary>
    /// 管理するオブジェクトインスタンスの構造体
    /// </summary>
    struct Instance {
        int id = -1; // オブジェクトID（StageObjectのIDと対応）
        std::unique_ptr<Object3d> object; // 描画用オブジェクト
    };

    // Object3d 初期化用
    Object3dCommon* object3dCommon_ = nullptr;

    // 描画時に使用するカメラ
    Camera* camera_ = nullptr;

    // 現在描画中のオブジェクト一覧
    std::vector<Instance> instances_;
};
