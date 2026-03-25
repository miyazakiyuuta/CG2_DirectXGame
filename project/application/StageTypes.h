#pragma once
#pragma once
#include <string>
#include <vector>
#include "math/Vector3.h"
#include "math/Vector4.h"

// ブロック種類の列挙
enum class BlockID {
    Normal = 0,    // 通常ブロック
    Water = 1,     // 水ブロック（回復など用途）
    BugSpawn = 2,  // 虫のスポーン位置用
    PlayerSpawn = 3, // プレイヤー開始位置用
};

/// <summary>
/// ステージ内のオブジェクトを表す構造体
/// (元の StageEditor.h 内定義をそのまま移植)
/// </summary>
struct StageObject {

    // 一意の識別ID
    int id = -1;

    // 使用するモデルファイル名 (例: "Cube.obj")
    std::string modelName;

    // ワールド座標
    Vector3 position {};

    // オイラー角（ラジアン想定）
    Vector3 rotation {};

    // 拡縮率（初期値は等倍）
    Vector3 scale { 1.0f, 1.0f, 1.0f };

    // ブロックの種類
    BlockID blockId = BlockID::Normal;

    // 表示色 (RGBA)
    Vector4 color { 1.0f, 1.0f, 1.0f, 1.0f };
};

/// <summary>
/// ステージ全体のデータ構造
/// </summary>
struct StageData {

    // ステージ名（ファイル名UI表示用など）
    std::string name;

    // ステージ内の全オブジェクト
    std::vector<StageObject> objects;
};
