#pragma once

class ITransition {
public:
	virtual ~ITransition() = default;
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;

	virtual bool IsReadyToChange() const = 0;
	virtual bool IsFinished() const = 0;
};