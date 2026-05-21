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
};

struct TutorialTask {
	std::string title;
	std::string message;

	std::function<bool(const TutorialContext&)> isCompleted;
	std::function<void(const TutorialContext&)> onEnter;

	float completeWaitSeconds = 0.0f;
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