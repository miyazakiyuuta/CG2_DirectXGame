#pragma once
#include <string>

// ログ出力
namespace Logger {
	void Initialize();
	void Finalize();
	void Log(const std::string& message);
}

