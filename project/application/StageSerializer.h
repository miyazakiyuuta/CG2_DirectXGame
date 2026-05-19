#pragma once

#include <optional>
#include <string>
#include <vector>
#include "StageTypes.h"

/// <summary>
/// ステージデータのシリアライズとデシリアライズを担当するクラス
/// </summary>
class StageSerializer {
public:

    /// <summary>
    /// JSONファイルに StageData を保存
    /// </summary>
    // SaveToFile: savedPosition は移動床 (MovingPlatform) のみのメタデータとして扱われます。
    // savedPositionPersisted が true のオブジェクトは、以降の保存で position による上書きから保護されます。
    static bool SaveToFile(const StageData& data, const std::string& path);

    /// <summary>
    /// JSONファイルから StageData を読み込み
    /// </summary>
    static std::optional<StageData> LoadFromFile(const std::string& path);
};
