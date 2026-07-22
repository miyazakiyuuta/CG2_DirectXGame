#include "input/ActionInput.h"

#include "io/Input.h"

#include <algorithm>

namespace {
	// RT(アナログトリガー)を射撃ボタンとみなす押し込み量のしきい値
	constexpr float kTriggerThreshold = 0.5f;
}

float ActionInput::GetMoveX() const {
	Input* input = Input::GetInstance();

	float axis = 0.0f;
	if (input->IsPushKey(DIK_A) || input->IsPushKey(DIK_LEFT)) {
		axis -= 1.0f;
	}
	if (input->IsPushKey(DIK_D) || input->IsPushKey(DIK_RIGHT)) {
		axis += 1.0f;
	}
	// スティックはデッドゾーン処理済みの -1〜+1 が返る(Input::NormalizeStick)
	axis += input->GetLeftStickX();

	return std::clamp(axis, -1.0f, 1.0f);
}

float ActionInput::GetMoveY() const {
	Input* input = Input::GetInstance();

	float axis = 0.0f;
	if (input->IsPushKey(DIK_S) || input->IsPushKey(DIK_DOWN)) {
		axis -= 1.0f;
	}
	if (input->IsPushKey(DIK_W) || input->IsPushKey(DIK_UP)) {
		axis += 1.0f;
	}
	// XInputの左スティックYは上が+なのでそのまま加算できる
	axis += input->GetLeftStickY();

	return std::clamp(axis, -1.0f, 1.0f);
}

bool ActionInput::IsPress(Action action) const {
	Input* input = Input::GetInstance();

	switch (action) {
	case Action::Shoot:
		return input->IsPushKey(DIK_SPACE) || input->IsPressMouse(0) ||
			input->IsPressPad(XINPUT_GAMEPAD_A) ||
			input->GetRightTrigger() >= kTriggerThreshold;
	case Action::Boost:
		return input->IsPushKey(DIK_LSHIFT) || input->IsPressPad(XINPUT_GAMEPAD_B);
	}
	return false;
}

bool ActionInput::IsTrigger(Action action) const {
	Input* input = Input::GetInstance();

	switch (action) {
	case Action::Shoot:
		// RTはアナログ値のため瞬間判定は未対応(前回値の保持が必要)。
		// 連射制御は⑪でクールタイム方式にする予定なのでIsPress側で足りる見込み
		return input->IsTriggerKey(DIK_SPACE) || input->IsTriggerMouse(0) ||
			input->IsTriggerPad(XINPUT_GAMEPAD_A);
	case Action::Boost:
		return input->IsTriggerKey(DIK_LSHIFT) || input->IsTriggerPad(XINPUT_GAMEPAD_B);
	}
	return false;
}
