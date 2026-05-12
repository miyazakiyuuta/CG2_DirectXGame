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
    Breakable = 4, // 破壊可能ブロック
    Warp = 5,      // ワープブロック
    MovingPlatform = 6, // 動く床ブロック
    EnemySpawn = 7,
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

    // --- 以下は各種ブロック固有プロパティ（デフォルトは無効／未設定） ---
    // 破壊可能ブロック用の体力（0 = 無し）
    int hp = 0;

    // ワープブロック用のターゲット位置（ワールド座標）
    Vector3 warpTargetPosition {};
    // ワープ先シーンID（-1 = 同一シーン）
    int warpTargetSceneId = -1;

    // 動く床用プロパティ
    // 移動方向: 0 = none, 1 = up, 2 = down, 3 = left, 4 = right
    int moveDirection = 0;
    // 移動速度
    float moveSpeed = 0.0f;
    // 移動距離（往復幅など）
    float moveRange = 0.0f;
    // 初期位相（0.0 - 1.0）: サイン波の初期位相を正規化して指定
    float movePhase = 0.0f;

    // 敵スポーン用
    int enemyType = 0; // EnemyType::Chasing 相当をデフォルト

    // 敵リスポーン用（秒）
    float enemyRespawnInterval = 5.0f;
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

struct EnemySpawnPoint{
    Vector3 position{};
    int enemyType = 0;
    float respawnInterval = 5.0f;
};
