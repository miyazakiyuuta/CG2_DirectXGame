#include "StageSerializer.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <../externals/nlohmann/json.hpp>

namespace {
    std::vector<std::string> GetEnemyFileCandidates(const std::string& stagePathStr)
    {
        std::vector<std::string> result;
        std::filesystem::path stagePath(stagePathStr);
        // Preferred: <stem>_enemies.json (e.g. resources/stage.json -> resources/stage_enemies.json)
        result.push_back((stagePath.parent_path() / (stagePath.stem().string() + "_enemies.json")).string());
        return result;
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

        // オブジェクトの色を保存
        jo["color"] = color;

        // 位置を保存
        nlohmann::json pos;
        pos["x"] = o.position.x;
        pos["y"] = o.position.y;
        pos["z"] = o.position.z;

        // 回転を保存
        nlohmann::json rot;
        rot["x"] = o.rotation.x;
        rot["y"] = o.rotation.y;
        rot["z"] = o.rotation.z;

        // 拡縮率を保存
        nlohmann::json scl;
        scl["x"] = o.scale.x;
        scl["y"] = o.scale.y;
        scl["z"] = o.scale.z;

        // 位置・回転・拡縮率をオブジェクトに保存
        jo["position"] = pos;
        jo["rotation"] = rot;
        jo["scale"] = scl;

        // block 固有プロパティを保存
        jo["hp"] = o.hp;

        nlohmann::json warpTarget;
        warpTarget["x"] = o.warpTargetPosition.x;
        warpTarget["y"] = o.warpTargetPosition.y;
        warpTarget["z"] = o.warpTargetPosition.z;
        jo["warpTargetPosition"] = warpTarget;
        jo["warpTargetSceneId"] = o.warpTargetSceneId;

        jo["moveDirection"] = o.moveDirection;
        jo["moveSpeed"] = o.moveSpeed;
        jo["moveRange"] = o.moveRange;
        jo["movePhase"] = o.movePhase;
        jo["enemyType"] = o.enemyType;
        jo["enemyRespawnInterval"] = o.enemyRespawnInterval;

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
        // EnemySpawn は別ファイルにも書き出しておく（編集・差し替え用）
        // 例: resources/stage.json -> resources/stage_enemies.json
        nlohmann::json je = nlohmann::json::array();
        for (const auto& o : data.objects) {
            if (o.blockId != BlockID::EnemySpawn) {
                continue;
            }
            nlohmann::json se;
            se["id"] = o.id;
            nlohmann::json pos;
            pos["x"] = o.position.x;
            pos["y"] = o.position.y;
            pos["z"] = o.position.z;
            se["position"] = pos;
            se["enemyType"] = o.enemyType;
            se["enemyRespawnInterval"] = o.enemyRespawnInterval;
            je.push_back(se);
        }

        const auto candidates = GetEnemyFileCandidates(path);
        for (const auto& enemyPath : candidates) {
            (void)TryWriteJsonFile(enemyPath, je);
        }
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
                    o.modelName = "Cube.obj";
                    o.scale = { 1.0f, 1.0f, 1.0f };
                    o.color = { 0.0f, 1.0f, 0.0f, 1.0f };

                    if (item.contains("id")) {
                        o.id = item["id"].get<int>();
                    } else {
                        o.id = nextId++;
                    }

                    if (item.contains("position")) {
                        o.position.x = item["position"].value("x", 0.0f);
                        o.position.y = item["position"].value("y", 0.0f);
                        o.position.z = item["position"].value("z", 0.0f);
                    } else {
                        o.position.x = item.value("x", 0.0f);
                        o.position.y = item.value("y", 0.0f);
                        o.position.z = item.value("z", 0.0f);
                    }

                    o.enemyType = item.value("enemyType", 0);
                    if (item.contains("enemyRespawnInterval")) {
                        o.enemyRespawnInterval = item["enemyRespawnInterval"].get<float>();
                    } else if (item.contains("respawnInterval")) {
                        o.enemyRespawnInterval = item["respawnInterval"].get<float>();
                    }
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
