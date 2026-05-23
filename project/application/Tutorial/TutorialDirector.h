#pragma once

#include <functional>
#include <string>
#include <vector>

class Player;
class CameraController;
class Input;

struct TutorialContext {
	Player* player = nullptr;
	CameraController* cameraController = nullptr;
	Input* input = nullptr;
	float deltaTime = 1.0f / 60.0f;

	bool tongueShotStartedThisFrame = false;
};

struct TutorialTask {
	std::string title;
	std::string message;

	// このスコアまで溜まったら達成
	int requiredScore = 1;

	// 毎フレーム呼ばれ、加算するスコアを返す
	// 例: ジャンプ入力があれば1、なければ0
	std::function<int(const TutorialContext&)> scoreDelta;

	// タスク開始時に一度だけ呼ばれる
	std::function<void(const TutorialContext&)> onEnter;

	// 達成後、少し待ってから次へ進む
	float completeWaitSeconds = 0.0f;

	int currentScore = 0;

	std::function<void(const TutorialContext&)> onExit;
};

class TutorialDirector {
public:
	void Clear();
	void AddTask(const TutorialTask& task);

	void Start();
	void Update(const TutorialContext& context);

	bool IsFinished() const;
	int GetCurrentIndex() const { return currentIndex_; }

	const TutorialTask* GetCurrentTask() const;
	const std::string& GetCurrentTitle() const;
	const std::string& GetCurrentMessage() const;

	int GetCurrentScore() const;
	int GetCurrentRequiredScore() const;
	float GetCurrentProgress() const;

private:
	void EnterCurrentTask(const TutorialContext& context);

private:
	std::vector<TutorialTask> tasks_;

	int currentIndex_ = 0;
	bool started_ = false;
	bool currentTaskEntered_ = false;
	bool waitingComplete_ = false;
	float completeWaitTimer_ = 0.0f;

	std::string emptyText_;
};