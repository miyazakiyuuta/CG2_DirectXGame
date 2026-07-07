#include <Windows.h>
#include "base/CrashDump.h"
#include "base/D3DResourceLeakChecker.h"
#include "Game.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	CrashDump::Register();
	D3DResourceLeakChecker leakCheck;

	Game game;
	game.Run();

	return 0;
}