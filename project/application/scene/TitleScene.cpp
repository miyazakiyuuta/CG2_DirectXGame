#include "scene/TitleScene.h"
#include "io/Input.h"
#include "scene/SceneManager.h"
#include "transition/ShutterTransition.h"
#include "memory.h"

void TitleScene::Initialize() {
}

void TitleScene::Finalize() {
}

void TitleScene::Update() {
	if (Input::GetInstance()->TriggerKey(DIK_RETURN)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY", std::make_unique<ShutterTransition>());
	}
}

void TitleScene::Draw() {
}
