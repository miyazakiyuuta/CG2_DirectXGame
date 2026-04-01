#pragma once
#include "math/Vector3.h"

class Bug;

// 挙動の共通ルール
class IBugBehavior {
public:
	virtual ~IBugBehavior() = default;
	virtual void Initialize(Bug* bug) = 0;
	virtual void Update(Bug* bug) = 0;
	virtual bool IsFinished(Bug* bug) const = 0;
};

// 挙動1：まっすぐ飛ぶ（加減速付き）
class BehaviorStraight : public IBugBehavior {
private:
	Vector3 targetPos_ = {};
	float maxSpeed_ = 0.0f;

public:
	void Initialize(Bug* bug) override;
	void Update(Bug* bug) override;
	bool IsFinished(Bug* bug) const override;
};

// 挙動2：ホバリング（その場で漂う）
class BehaviorHover : public IBugBehavior {
private:
	float timer_ = 0.0f;
	float duration_ = 0.0f;

public:
	void Initialize(Bug* bug) override;
	void Update(Bug* bug) override;
	bool IsFinished(Bug* bug) const override;
};

// 挙動3：ベジェ曲線（滑らかなカーブ移動）
class BehaviorCurve : public IBugBehavior {
private:
	Vector3 startPos_ = {};
	Vector3 controlPos_ = {};
	Vector3 endPos_ = {};
	float t_ = 0.0f;
	float tSpeed_ = 0.0f;

public:
	void Initialize(Bug* bug) override;
	void Update(Bug* bug) override;
	bool IsFinished(Bug* bug) const override;
};
