#pragma once
#include "math/Transform.h"

#include <string>
#include <vector>

// シーン内オブジェクトのTransformをJSONファイルへ保存/復元するヘルパー。
// ポインタは保存できないため、nameをキーにJSONと照合して書き戻す方式。
namespace SceneSerializer {
	// 保存/復元の対象1件(nameはEditorObjectと揃えておくと迷わない)
	struct Entry {
		std::string name;
		Transform* transform = nullptr;
	};

	/// <summary>
	/// entriesの内容をJSONファイルへ保存する(ディレクトリが無ければ作る)
	/// </summary>
	/// <returns>成功したら true</returns>
	bool Save(const std::string& path, const std::vector<Entry>& entries);

	/// <summary>
	/// JSONファイルを読み、nameが一致するentryのTransformへ書き戻す。
	/// ファイルが無い場合は何もしない(初回起動の正常系)
	/// </summary>
	/// <returns>復元できたら true</returns>
	bool Load(const std::string& path, const std::vector<Entry>& entries);
}
