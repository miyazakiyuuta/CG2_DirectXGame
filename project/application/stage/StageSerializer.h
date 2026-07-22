#pragma once
#include "stage/StageData.h"

#include <string>

// stage.json <-> StageData の変換を担当するシリアライザ。
// JSON形式: {"objects":[{"type":"static", "name":..., "model":..., "transform":{...},
//            "disabled":(省略可), "collider":(省略可)}, ...],
//            "rail":(省略可), "camera":{"fovY":..., "railSpeed":..., "backDistance":...}(省略可)}
namespace StageSerializer {
	/// <summary>
	/// ステージデータファイルを読み込む。
	/// stage.jsonはゲームの必須データなので、開けない場合は警告ログを残す
	/// </summary>
	/// <param name="path">作業ディレクトリ(プロジェクト直下)からの相対パス</param>
	/// <param name="stageData">読み込み先(失敗時は書き換えない)</param>
	/// <returns>読み込めたら true</returns>
	bool Load(const std::string& path, StageData& stageData);

	/// <summary>
	/// ステージデータをファイルへ書き出す。
	/// 省略可能フィールド(disabled/collider)は既定値なら書かない(Loadの省略時既定値と対称)
	/// </summary>
	/// <param name="path">作業ディレクトリ(プロジェクト直下)からの相対パス</param>
	/// <param name="stageData">書き出すデータ</param>
	/// <returns>書き出せたら true</returns>
	bool Save(const std::string& path, const StageData& stageData);
}
