#include "UI/GameTimer.h"

#include <algorithm>
#include <cmath>

void GameTimer::StartCountUp()
{
    mode_ = Mode::CountUp;
    timeSeconds_ = 0.0f;
    paused_ = false;
    finished_ = false;
}

void GameTimer::StartCountDown(float seconds)
{
    mode_ = Mode::CountDown;
    timeSeconds_ = std::max(0.0f, seconds);
    paused_ = false;
    finished_ = false;
}

void GameTimer::Reset()
{
    timeSeconds_ = 0.0f;
    paused_ = false;
    finished_ = false;
}

void GameTimer::Update(float deltaTime)
{
    if (paused_ || finished_) {
        return;
    }

    if (mode_ == Mode::CountUp) {
        timeSeconds_ += deltaTime;
        return;
    }

    timeSeconds_ -= deltaTime;
    if (timeSeconds_ <= 0.0f) {
        timeSeconds_ = 0.0f;
        finished_ = true;
    }
}

int GameTimer::GetDisplaySeconds() const
{
    if (mode_ == Mode::CountUp) {
        return static_cast<int>(std::floor(timeSeconds_));
    }

    return static_cast<int>(std::ceil(timeSeconds_));
}