#pragma once
#include <Windows.h>

/// <summary>
/// クラッシュ(未捕捉のSEH例外)発生時にminidumpを出力する仕組み
/// </summary>
class CrashDump {
public:
	/// <summary>
	/// 未捕捉例外ハンドラとして登録する。WinMainの先頭で一度呼ぶ
	/// </summary>
	static void Register();

private:
	/// <summary>
	/// SetUnhandledExceptionFilterに渡すコールバック。
	/// ./Dumps以下に日時付きの.dmpを書き出す
	/// </summary>
	static LONG WINAPI Export(EXCEPTION_POINTERS* exception);
};
