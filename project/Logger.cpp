#include "Logger.h"
#include <debugapi.h>
#include <filesystem> // ファイルやディレクトリに関する操作を行うライブラリ
#include <fstream> // ファイルに書いたり読んだりするライブラリ
#include <chrono> // 時間を扱うライブラリ


void Logger::Initialize() {

    //ログのディレクトリを用意
    const std::string logDir = "logs/";
    std::filesystem::create_directories(logDir);
    // 現在時刻を取得(UTC時刻)
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // ログファイルの名前にコンマ何秒はいらないので、削って秒にする
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> 
        nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    // 日本時間(PCの設定時間)に変換
    std::chrono::zoned_time localTime{
        std::chrono::current_zone(), nowSeconds
    };
    // formatを使って年月日_時分秒の文字に変換
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    // 時刻を使ってファイル名を決定
    std::string logFilePath = logDir + dateString + ".log";

    logStream.open(logFilePath, std::ios::out);
}

void Logger::Finalize() {
    if (logStream.is_open()) {
        logStream.close();
    }
}

void Logger::Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}
