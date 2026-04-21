#pragma once

#include <string>
#include <unordered_map>

/// プレイヤーが獲得できる能力の種類を表す列挙型
enum class AbilityId {
    Unknown = 0, // 不明な能力
    JumpPower, // ジャンプ力の増加
    TongueRange, // 舌の射程の増加
    SonarDuration, // ソナーの持続時間の増加
    WallClingDuration, // 壁に張り付ける時間の増加
    CamouflageDuration, // カモフラージュの持続時間の増加
};

/// <summary>
/// 文字列からAbilityIdを取得する。例えば "JumpPower" や "Jump" は AbilityId::JumpPower を返す。
/// </summary>
static inline AbilityId AbilityIdFromString(const std::string& s) {
    if (s == "JumpPower" || s == "Jump") return AbilityId::JumpPower;
    if (s == "TongueRange" || s == "Tongue") return AbilityId::TongueRange;
    if (s == "SonarDuration" || s == "Sonar") return AbilityId::SonarDuration;
    if (s == "WallClingDuration" || s == "WallCling") return AbilityId::WallClingDuration;
    if (s == "CamouflageDuration" || s == "Camouflage") return AbilityId::CamouflageDuration;
    return AbilityId::Unknown;
}


/// <summary>
/// AbilityIdを文字列に変換する。例えば AbilityId::JumpPower は "JumpPower" を返す。
/// </summary>
static inline const char* AbilityIdToString(AbilityId id) {
    switch (id) {
    case AbilityId::JumpPower: return "JumpPower";
    case AbilityId::TongueRange: return "TongueRange";
    case AbilityId::SonarDuration: return "SonarDuration";
    case AbilityId::WallClingDuration: return "WallClingDuration";
    case AbilityId::CamouflageDuration: return "CamouflageDuration";
    default: return "Unknown";
    }
}

namespace std {
template<> struct hash<AbilityId> {
    size_t operator()(AbilityId a) const noexcept { return std::hash<int>()(static_cast<int>(a)); }
};
}
