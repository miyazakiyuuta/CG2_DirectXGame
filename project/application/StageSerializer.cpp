#include "StageSerializer.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <../externals/nlohmann/json.hpp>

namespace {
    std::vector<std::string> GetStageDerivedFileCandidates(const std::string& stagePathStr, const std::string& suffix)
    {
        std::vector<std::string> result;
        std::filesystem::path stagePath(stagePathStr);
        result.push_back((stagePath.parent_path() / (stagePath.stem().string() + suffix)).string());
        return result;
    }

    std::vector<std::string> GetEnemyFileCandidates(const std::string& stagePathStr)
    {
        // Preferred: <stem>_enemies.json (e.g. resources/stage.json -> resources/stage_enemies.json)
        return GetStageDerivedFileCandidates(stagePathStr, "_enemies.json");
    }

    bool TryWriteJsonFile(const std::string& path, const nlohmann::json& j)
    {
        try {
            std::ofstream ofs(path);
            if (!ofs.is_open()) {
                return false;
            }
            ofs << std::setw(2) << j << std::endl;
            return true;
        } catch (...) {
            return false;
        }
    }
}

/// <summary>
/// JSONファイルに StageData を保存
/// </summary>
bool StageSerializer::SaveToFile(const StageData& data, const std::string& path)
{
    // StageData の内容を nlohmann::json オブジェクトに変換
    nlohmann::json j;
    // ステージ名を保存
    j["name"] = data.name;

        // オブジェクト配列を保存
        for (const auto& o : data.objects) {
            nlohmann::json jo;
            // オブジェクトの基本情報を保存
            jo["id"] = o.id;
            jo["modelName"] = o.modelName;
            jo["blockId"] = static_cast<int>(o.blockId);

            // 色を保存（RGBA）
            nlohmann::json color;
            color["x"] = o.color.x;
            color["y"] = o.color.y;
            color["z"] = o.color.z;
            color["w"] = o.color.w;
            jo["color"] = color;

            // 位置を保存
            nlohmann::json pos;
            // 移動床については savedPositionPersisted が true の場合 savedPosition を優先して保存する
            if (o.blockId == BlockID::MovingPlatform && o.savedPositionPersisted) {
                pos["x"] = o.savedPosition.x;
                pos["y"] = o.savedPosition.y;
                pos["z"] = o.savedPosition.z;
            } else {
                pos["x"] = o.position.x;
                pos["y"] = o.position.y;
                pos["z"] = o.position.z;
            }
            jo["position"] = pos;

            // 回転を保存
            nlohmann::json rot;
            rot["x"] = o.rotation.x;
            rot["y"] = o.rotation.y;
            rot["z"] = o.rotation.z;
            jo["rotation"] = rot;

            // 拡縮率を保存
            nlohmann::json scl;
            scl["x"] = o.scale.x;
            scl["y"] = o.scale.y;
            scl["z"] = o.scale.z;
            jo["scale"] = scl;

            // ブロック固有フィールドは該当するオブジェクトにのみ追加する
            if (o.blockId == BlockID::Breakable) {
                jo["hp"] = o.hp;
            }

            if (o.blockId == BlockID::Warp) {
                nlohmann::json warpTarget;
                warpTarget["x"] = o.warpTargetPosition.x;
                warpTarget["y"] = o.warpTargetPosition.y;
                warpTarget["z"] = o.warpTargetPosition.z;
                jo["warpTargetPosition"] = warpTarget;
                jo["warpTargetSceneId"] = o.warpTargetSceneId;
            }

            if (o.blockId == BlockID::MovingPlatform) {
                jo["moveDirection"] = o.moveDirection;
                jo["moveSpeed"] = o.moveSpeed;
                jo["moveRange"] = o.moveRange;
                jo["movePhase"] = o.movePhase;

                nlohmann::json savedPos;
                savedPos["x"] = o.savedPosition.x;
                savedPos["y"] = o.savedPosition.y;
                savedPos["z"] = o.savedPosition.z;
                jo["savedPosition"] = savedPos;
                jo["savedPositionPersisted"] = o.savedPositionPersisted;
                jo["movementLocked"] = o.movementLocked;
            }

            if (o.blockId == BlockID::EnemySpawn) {
                jo["enemyType"] = o.enemyType;
                jo["enemyRespawnInterval"] = o.enemyRespawnInterval;
                jo["allowRespawn"] = o.allowRespawn;
                jo["spawnOnSceneStart"] = o.spawnOnSceneStart;
            }

            // オブジェクトをオブジェクト配列に追加
            j["objects"].push_back(jo);
        }

    // JSONオブジェクトをファイルに保存
    std::ofstream ofs(path);
    // ファイルが開けない場合は保存失敗とする
    if (!ofs.is_open()) {
        return false;
    }

    // JSONを整形して保存（インデント2スペース）
    ofs << std::setw(2) << j << std::endl;

    // 正常に保存できた場合は true を返す
    try {
        // ブロック種別ごとの補助ファイルを書き出す（編集・差し替え用）
        // 例: resources/stage.json ->
        //   resources/stage_enemies.json
        //   resources/stage_moving_platforms.json
        //   resources/stage_warps.json
        //   resources/stage_breakables.json

        // EnemySpawn: export full enemy-spawn data so enemies file is authoritative
        nlohmann::json je = nlohmann::json::array();
        for (const auto& o : data.objects) {
            if (o.blockId != BlockID::EnemySpawn) {
                continue;
            }
            nlohmann::json se;
            // id and model
            se["id"] = o.id;
            se["modelName"] = o.modelName;

            // position
            nlohmann::json pos;
            pos["x"] = o.position.x;
            pos["y"] = o.position.y;
            pos["z"] = o.position.z;
            se["position"] = pos;

            // scale
            nlohmann::json scl;
            scl["x"] = o.scale.x;
            scl["y"] = o.scale.y;
            scl["z"] = o.scale.z;
            se["scale"] = scl;

            // color
            nlohmann::json col;
            col["x"] = o.color.x;
            col["y"] = o.color.y;
            col["z"] = o.color.z;
            col["w"] = o.color.w;
            se["color"] = col;

            // enemy-specific
            se["enemyType"] = o.enemyType;
            se["enemyRespawnInterval"] = o.enemyRespawnInterval;
            // keep old key name too for compatibility
            se["respawnInterval"] = o.enemyRespawnInterval;
            se["allowRespawn"] = o.allowRespawn;
            se["spawnOnSceneStart"] = o.spawnOnSceneStart;

            je.push_back(se);
        }
        for (const auto& enemyPath : GetStageDerivedFileCandidates(path, "_enemies.json")) {
            (void)TryWriteJsonFile(enemyPath, je);
        }
        // NOTE: only enemy auxiliary file is written by design; moving platforms, warps and breakables
        // are intentionally not exported to separate files anymore.
    } catch (...) {
    }

    return true;
}

/// <summary>
/// JSONファイルから StageData を読み込み
/// </summary>
std::optional<StageData> StageSerializer::LoadFromFile(const std::string& path)
{
    // ファイルが存在しない場合は読み込み失敗とする
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    // JSONファイルを開く
    std::ifstream ifs(path);

    // ファイルが開けない場合は読み込み失敗とする
    if (!ifs.is_open()) {
        return std::nullopt;
    }

    // JSONファイルを nlohmann::json オブジェクトに読み込む
    try {
        // JSONの内容を読み込む
        nlohmann::json j;
        ifs >> j;
        StageData data;

        // ステージ名を読み込む
        if (j.contains("name")) {
            data.name = j["name"].get<std::string>();
        }

        // オブジェクト配列を読み込む
        if (j.contains("objects")) {
            // オブジェクト配列が存在する場合は、各オブジェクトを StageObject に変換して StageData に追加する
            for (auto& jo : j["objects"]) {
                StageObject o;

                // オブジェクトの基本情報を読み込む
                if (jo.contains("id")) {
                    o.id = jo["id"].get<int>();
                }

                // モデル名を読み込む
                if (jo.contains("modelName")) {
                    o.modelName = jo["modelName"].get<std::string>();
                }

                // ブロックIDを読み込む
                if (jo.contains("blockId")) {
                    o.blockId = static_cast<BlockID>(jo["blockId"].get<int>());
                }

                // 色を読み込む
                if (jo.contains("color")) {
                    o.color.x = jo["color"]["x"].get<float>();
                    o.color.y = jo["color"]["y"].get<float>();
                    o.color.z = jo["color"]["z"].get<float>();
                    o.color.w = jo["color"]["w"].get<float>();
                }

                // 位置を読み込む
                if (jo.contains("position")) {
                    o.position.x = jo["position"]["x"].get<float>();
                    o.position.y = jo["position"]["y"].get<float>();
                    o.position.z = jo["position"]["z"].get<float>();
                }

                // 回転を読み込む
                if (jo.contains("rotation")) {
                    o.rotation.x = jo["rotation"]["x"].get<float>();
                    o.rotation.y = jo["rotation"]["y"].get<float>();
                    o.rotation.z = jo["rotation"]["z"].get<float>();
                }

                // 拡縮率を読み込む
                if (jo.contains("scale")) {
                    o.scale.x = jo["scale"]["x"].get<float>();
                    o.scale.y = jo["scale"]["y"].get<float>();
                    o.scale.z = jo["scale"]["z"].get<float>();
                }

                // block 固有プロパティの読み込み（存在しなければデフォルト値のまま）
                if (jo.contains("hp")) {
                    o.hp = jo["hp"].get<int>();
                }

                if (jo.contains("warpTargetPosition")) {
                    o.warpTargetPosition.x = jo["warpTargetPosition"]["x"].get<float>();
                    o.warpTargetPosition.y = jo["warpTargetPosition"]["y"].get<float>();
                    o.warpTargetPosition.z = jo["warpTargetPosition"]["z"].get<float>();
                }
                if (jo.contains("warpTargetSceneId")) {
                    o.warpTargetSceneId = jo["warpTargetSceneId"].get<int>();
                }

                if (jo.contains("moveDirection")) {
                    o.moveDirection = jo["moveDirection"].get<int>();
                }
                if (jo.contains("moveSpeed")) {
                    o.moveSpeed = jo["moveSpeed"].get<float>();
                }
                if (jo.contains("moveRange")) {
                    o.moveRange = jo["moveRange"].get<float>();
                }
                if(jo.contains("movePhase")){
                    o.movePhase = jo["movePhase"].get<float>();
                }
                // savedPosition / persisted は移動床専用のメタデータとしてのみ読み込む
                if (o.blockId == BlockID::MovingPlatform) {
                    if (jo.contains("savedPosition")) {
                        if (jo["savedPosition"].contains("x")) o.savedPosition.x = jo["savedPosition"]["x"].get<float>();
                        if (jo["savedPosition"].contains("y")) o.savedPosition.y = jo["savedPosition"]["y"].get<float>();
                        if (jo["savedPosition"].contains("z")) o.savedPosition.z = jo["savedPosition"]["z"].get<float>();
                    }
                    if (jo.contains("savedPositionPersisted")) {
                        o.savedPositionPersisted = jo["savedPositionPersisted"].get<bool>();
                    }
                    if (jo.contains("movementLocked")) {
                        o.movementLocked = jo["movementLocked"].get<bool>();
                    }
                }
                if(jo.contains("enemyType")){
                    o.enemyType = jo["enemyType"].get<int>();
                }
                if(jo.contains("enemyRespawnInterval")){
                    o.enemyRespawnInterval = jo["enemyRespawnInterval"].get<float>();
                }

                // 読み込んだオブジェクトを StageData に追加
                data.objects.push_back(o);
            }
        }

        // 正常に読み込めた場合は StageData を返す
        // EnemySpawn の上書き（敵スポーン情報を別ファイルから読み込む）
        try {
            for (const auto& enemyPath : GetEnemyFileCandidates(path)) {
                if (!std::filesystem::exists(enemyPath)) {
                    continue;
                }
                std::ifstream eifs(enemyPath);
                if (!eifs.is_open()) {
                    continue;
                }

                nlohmann::json je;
                eifs >> je;
                if (!je.is_array()) {
                    continue;
                }

                // 別ファイルが見つかったら、stage.json 側の EnemySpawn は無視して差し替える
                // しかし既存の EnemySpawn に設定された modelName を保持できるようにする
                std::unordered_map<int, StageObject> existingEnemyMap;
                std::vector<std::pair<int, Vector3>> existingEnemyPositions;
                for (const auto& o : data.objects) {
                    if (o.blockId == BlockID::EnemySpawn) {
                        existingEnemyMap[o.id] = o;
                        existingEnemyPositions.push_back({o.id, o.position});
                    }
                }

                data.objects.erase(
                    std::remove_if(data.objects.begin(), data.objects.end(), [](const StageObject& o) {
                        return o.blockId == BlockID::EnemySpawn;
                    }),
                    data.objects.end());

                int maxId = 0;
                for (const auto& o : data.objects) {
                    maxId = (std::max)(maxId, o.id);
                }
                int nextId = maxId + 1;

                for (const auto& item : je) {
                    StageObject o;
                    o.blockId = BlockID::EnemySpawn;

                    // id
                    if (item.contains("id")) {
                        o.id = item["id"].get<int>();
                    } else {
                        o.id = nextId++;
                    }

                    // modelName
                    o.modelName = item.value("modelName", std::string("Cube.obj"));

                    // position
                    if (item.contains("position")) {
                        o.position.x = item["position"].value("x", 0.0f);
                        o.position.y = item["position"].value("y", 0.0f);
                        o.position.z = item["position"].value("z", 0.0f);
                    }

                    // scale
                    if (item.contains("scale")) {
                        o.scale.x = item["scale"].value("x", 1.0f);
                        o.scale.y = item["scale"].value("y", 1.0f);
                        o.scale.z = item["scale"].value("z", 1.0f);
                    }

                    // color
                    if (item.contains("color")) {
                        o.color.x = item["color"].value("x", 0.0f);
                        o.color.y = item["color"].value("y", 1.0f);
                        o.color.z = item["color"].value("z", 0.0f);
                        o.color.w = item["color"].value("w", 1.0f);
                    }

                    // enemy specific
                    o.enemyType = item.value("enemyType", 0);
                    if (item.contains("enemyRespawnInterval")) {
                        o.enemyRespawnInterval = item["enemyRespawnInterval"].get<float>();
                    } else if (item.contains("respawnInterval")) {
                        o.enemyRespawnInterval = item["respawnInterval"].get<float>();
                    }
                    o.allowRespawn = item.value("allowRespawn", true);
                    o.spawnOnSceneStart = item.value("spawnOnSceneStart", true);

                    if (o.enemyRespawnInterval < 0.0f) {
                        o.enemyRespawnInterval = 0.0f;
                    }

                    data.objects.push_back(o);
                }
                break; // first valid enemies file wins
            }
        } catch (...) {
            // ignore
        }

        return data;

    } catch (...) {
        // 例外が発生した場合は読み込み失敗とする
        return std::nullopt;
    }
}
