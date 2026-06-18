#include "scene/TitleScene.h"
#include "io/Input.h"
#include "scene/SceneManager.h"
#include "transition/ShutterTransition.h"
#include "transition/BlindTransition.h"
#include "effect/EffectManager.h"

void TitleScene::Initialize() {
}

void TitleScene::Finalize() {
}

void TitleScene::Update() {
	if (Input::GetInstance()->IsTriggerKey(DIK_RETURN)) {
		//SceneManager::GetInstance()->ChangeScene("GAMEPLAY", std::make_unique<ShutterTransition>());
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY", std::make_unique<BlindTransition>());
	}
}

void TitleScene::Draw() {
}
