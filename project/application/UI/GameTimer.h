#pragma once

class GameTimer {
public:
    enum class Mode {
        CountUp,
        CountDown,
    };

    void StartCountUp();
    void StartCountDown(float seconds);
    void Reset();

    void Update(float deltaTime);

    void SetPaused(bool paused) { paused_ = paused; }
    bool IsPaused() const { return paused_; }

    float GetTimeSeconds() const { return timeSeconds_; }
    int GetDisplaySeconds() const;

    bool IsFinished() const { return finished_; }

private:
    Mode mode_ = Mode::CountUp;
    float timeSeconds_ = 0.0f;
    bool paused_ = false;
    bool finished_ = false;
};