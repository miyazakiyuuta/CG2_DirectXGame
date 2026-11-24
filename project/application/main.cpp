#include <Windows.h>
#include "base/CrashDump.h"
#include "base/D3DResourceLeakChecker.h"
#include "Game.h"

#include <memory>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	CrashDump::Register();
	D3DResourceLeakChecker leakCheck;

	// 基底クラス(Framework)のポインタで持つ。Runは基底の関数で、その中から呼ばれる
	// Initialize/Update/Draw/Finalize(仮想関数)が、実体の型(Game)の版に振り分けられる。
	// 破棄もこの基底のポインタ経由になるので、Frameworkのデストラクタは仮想にしてある(Gameのメンバが解放されるように)
	std::unique_ptr<Framework> game = std::make_unique<Game>();
	game->Run();

	return 0;
}