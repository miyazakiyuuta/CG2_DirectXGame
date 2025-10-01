#include "DebugCamera.h"

using namespace MatrixMath;

void DebugCamera::Initialize() {
    pivot_ = {};
    matRot_ = MakeIdentity4x4();
	translation_ = { 0,0,-10 };
}

void DebugCamera::Update(IDirectInputDevice8* keyboard, POINT mouseDelta, float wheelDelta) {

    BYTE key[256] = {};
    // キーボードの状態を取得
    keyboard->Acquire(); // 入力受付状態にする
    keyboard->GetDeviceState(sizeof(key), key);

    // 追加回転分の回転行列の生成
    Matrix4x4 matRotDelta = MakeIdentity4x4();
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
        matRotDelta = Multiply(matRotDelta, MakeRotateXMatrix(mouseDelta.y * 0.005f));
        matRotDelta = Multiply(matRotDelta, MakeRotateYMatrix(mouseDelta.x * 0.005f));
    }

    Vector3 camToPivot = {
        translation_.x - pivot_.x,
        translation_.y - pivot_.y,
        translation_.z - pivot_.z,
    };

    camToPivot = TransformMatrix(camToPivot, matRotDelta);

    translation_.x = pivot_.x + camToPivot.x;
    translation_.y = pivot_.y + camToPivot.y;
    translation_.z = pivot_.z + camToPivot.z;

    // 累積の回転行列を合成
    //matRot_ = Multiply(matRotDelta, matRot_);
    matRot_ = Multiply(matRot_, matRotDelta);

    const float panSpeed = 0.01f;
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
        // 画面 X→右、Y→下 をカメラローカルに合わせてワールドへ変換
        Vector3 panLocal = {
            -mouseDelta.x * panSpeed,   // 画面右にドラッグで左にパン
             mouseDelta.y * panSpeed,   // 画面下にドラッグで上にパン
             0
        };
        Vector3 panWorld = TransformMatrix(panLocal, matRot_);

        // translation_ と pivot_ を一緒に動かす（ターゲットを中心に移動させる）
        translation_.x += panWorld.x;
        translation_.y += panWorld.y;
        translation_.z += panWorld.z;

        pivot_.x += panWorld.x;
        pivot_.y += panWorld.y;
        pivot_.z += panWorld.z;
    }

    const float zoomSpeed = 1.0f;
    if (wheelDelta != 0) {
        // カメラローカル Z 軸（奥方向）をワールドに変換
        Vector3 forward = TransformMatrix({ 0, 0, wheelDelta * zoomSpeed }, matRot_);
        translation_.x += forward.x;
        translation_.y += forward.y;
        translation_.z += forward.z;
    }

    Matrix4x4 translateMatrix = MakeTranslateMatrix(translation_);
    Matrix4x4 worldMatrix = Multiply(matRot_, translateMatrix);
    viewMatrix_ = Inverse(worldMatrix);

    //if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
    //    rotation_.x += mouseDelta.y * 0.001f; // 上下（pitch）
    //    rotation_.y += mouseDelta.x * 0.001f; // 左右（yaw）
    //}
    
    // カメラの回転行列
    //Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotation_.x);
    //Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotation_.y);
    //Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotation_.z);
    //Matrix4x4 rotateMatrix = Multiply(Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
    //Vector3 worldMove = TransformMatrix(move, rotateMatrix);
    //translation_.x += worldMove.x;
    //translation_.y += worldMove.y;
    //translation_.z += worldMove.z;
    // カメラの座標
    //Matrix4x4 translateMatrix = MakeTranslateMatrix(translation_);
    // ワールド→ビュー変換行列（カメラの逆変換）
    //Matrix4x4 cameraMatrix = Multiply(rotateMatrix, translateMatrix);
    // ビュー行列はカメラ行列の逆行列
    //viewMatrix_ = Inverse(cameraMatrix);

    
}
