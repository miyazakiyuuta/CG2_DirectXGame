#pragma once

#include <optional>
#include <string>
#include "StageTypes.h"

/// <summary>
/// ステージデータのシリアライズとデシリアライズを担当するクラス
/// </summary>
class StageSerializer {
public:

    /// <summary>
    /// JSONファイルに StageData を保存
    /// </summary>
    static bool SaveToFile(const StageData& data, const std::string& path);

    /// <summary>
    /// JSONファイルから StageData を読み込み
    /// </summary>
    static std::optional<StageData> LoadFromFile(const std::string& path);
};
