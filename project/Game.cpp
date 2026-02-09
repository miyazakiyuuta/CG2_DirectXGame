#include "Game.h"
#include "Logger.h"
#include "BaseScene.h"
#include "SceneFactory.h"


void Game::Initialize() {
	Framework::Initialize();

	sceneFactory_ = new SceneFactory();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_);
	SceneManager::GetInstance()->ChangeScene("TITLE");

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");
}

void Game::Finalize() {
	SceneManager::GetInstance()->~SceneManager();

	Framework::Finalize();
}

void Game::Update() {
	Framework::Update();
	SceneManager::GetInstance()->Update();
}

void Game::Draw() {
	Framework::Draw();

	SceneManager::GetInstance()->Draw();

}