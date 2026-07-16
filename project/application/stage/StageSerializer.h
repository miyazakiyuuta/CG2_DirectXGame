#pragma once
#include "stage/StageData.h"

#include <string>

// stage.json <-> StageData の変換を担当するシリアライザ。
// JSON形式: {"objects":[{"type":"static", "name":..., "model":..., "transform":{...},
//            "disabled":(省略可), "collider":(省略可)}, ...]}
// SaveはStageEditor実装時に追加する。
namespace StageSerializer {
	/// <summary>
	/// ステージデータファイルを読み込む。
	/// stage.jsonはゲームの必須データなので、開けない場合は警告ログを残す
	/// </summary>
	/// <param name="path">作業ディレクトリ(プロジェクト直下)からの相対パス</param>
	/// <param name="stageData">読み込み先(失敗時は書き換えない)</param>
	/// <returns>読み込めたら true</returns>
	bool Load(const std::string& path, StageData& stageData);
}
