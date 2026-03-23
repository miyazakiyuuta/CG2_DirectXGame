#pragma once

#include "3d/Camera.h"
#include "io/Input.h"
#include "math/Vector3.h"

/// <summary>
/// ステージエディタの入力制御クラス
/// </summary>
class EditorController {
public:
    /// <summary>
    /// エディタの入力制御クラス
    /// カメラ関連のユーティリティ（カメラ前方位置算出）や入力キーの判定を担当
    /// </summary>
    EditorController(Camera* camera);

    /// <summary>
    /// カメラの前方にオブジェクトを生成するための位置を計算して返す
    /// </summary>
    Vector3 GetCreatePosition(float distance = 5.0f) const;

    /// <summary>
    /// 生成キー（マウス右クリック）が押されたか
    /// </summary>
    bool IsCreateKeyPressed() const;

    /// <summary>
    /// 削除キー（Delete）が押されたか
    /// </summary>
    bool IsDeleteKeyPressed() const;

private:
    // 描画時に使用するカメラ
    Camera* camera_ = nullptr;
};
