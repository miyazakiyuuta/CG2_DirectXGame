#pragma once
#include <Windows.h>
#include <string>
#include <fstream>

// ログ出力
namespace Logger {
	static std::ofstream logStream;
	void Initialize();
	void Finalize();
	void Log(const std::string& message);
}

