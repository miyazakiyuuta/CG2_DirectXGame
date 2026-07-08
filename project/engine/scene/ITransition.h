#pragma once

class ITransition {
public:
	virtual ~ITransition() = default;
	virtual void Initialize() = 0;
	// deltaTimeは前フレームからの実経過時間[秒]
	virtual void Update(float deltaTime) = 0;
	virtual void Draw() = 0;

	virtual bool IsReadyToChange() const = 0;
	virtual bool IsFinished() const = 0;
};