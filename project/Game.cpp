#include "Game.h"
#include "Logger.h"
#include "BaseScene.h"
#include "SceneFactory.h"
#include "SceneManager.h"


void Game::Initialize() {
	Framework::Initialize();

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	//SceneManager::GetInstance()->ChangeScene("TITLE");
	SceneManager::GetInstance()->ChangeScene("GAMEPLAY");

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