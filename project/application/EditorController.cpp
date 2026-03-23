#include "EditorController.h"
#include <cmath>

/// <summary>
/// コンストラクタ
/// </summary>
EditorController::EditorController(Camera* camera)
    : camera_(camera)
{
}

/// <summary>
/// カメラの前方にオブジェクトを生成するための位置を計算して返す
/// </summary>
Vector3 EditorController::GetCreatePosition(float distance) const
{
    // カメラが有効でない場合は原点を返す
    if (!camera_) {
        return {};
    }

    // カメラの位置と回転を取得
    Vector3 pos = camera_->GetTranslate(); // カメラの位置
    Vector3 rot = camera_->GetRotate(); // カメラの回転（オイラー角）

    // 回転から前方ベクトルを計算
    float cp = std::cos(rot.x); // ピッチのコサイン
    float sp = std::sin(rot.x); // ピッチのサイン
    float cy = std::cos(rot.y); // ヨーのコサイン
    float sy = std::sin(rot.y); // ヨーのサイン

    // カメラの前方ベクトルを計算
    Vector3 forward = {
        -sy * cp,
        sp,
        -cy * cp
    };

    // 前方ベクトルを距離分だけ伸ばして、カメラ位置に加算して生成位置を算出
    return pos + forward * distance;
}

/// <summary>
/// 生成キー（マウス右クリック）が押されたか
/// </summary>
bool EditorController::IsCreateKeyPressed() const
{
    return Input::GetInstance()->IsTriggerMouse(1);
}

/// <summary>
/// 削除キー（Delete）が押されたか
/// </summary>
bool EditorController::IsDeleteKeyPressed() const
{
    return Input::GetInstance()->IsTriggerKey(DIK_DELETE);
}
