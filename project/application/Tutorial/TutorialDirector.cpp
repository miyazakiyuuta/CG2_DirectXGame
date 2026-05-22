#include "Tutorial/TutorialDirector.h"

void TutorialDirector::Clear()
{
	tasks_.clear();
	currentIndex_ = 0;
	started_ = false;
	currentTaskEntered_ = false;
	waitingComplete_ = false;
	completeWaitTimer_ = 0.0f;
}

void TutorialDirector::AddTask(const TutorialTask& task)
{
	tasks_.push_back(task);
}

void TutorialDirector::Start()
{
	currentIndex_ = 0;
	started_ = true;
	currentTaskEntered_ = false;
	waitingComplete_ = false;
	completeWaitTimer_ = 0.0f;
}

bool TutorialDirector::IsFinished() const
{
	return started_ && currentIndex_ >= static_cast<int>(tasks_.size());
}

const TutorialTask* TutorialDirector::GetCurrentTask() const
{
	if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(tasks_.size())) {
		return nullptr;
	}

	return &tasks_[currentIndex_];
}

const std::string& TutorialDirector::GetCurrentTitle() const
{
	const TutorialTask* task = GetCurrentTask();
	if (!task) {
		return emptyText_;
	}

	return task->title;
}

const std::string& TutorialDirector::GetCurrentMessage() const
{
	const TutorialTask* task = GetCurrentTask();
	if (!task) {
		return emptyText_;
	}

	return task->message;
}

int TutorialDirector::GetCurrentScore() const
{
	const TutorialTask* task = GetCurrentTask();
	if (!task) {
		return 0;
	}

	return task->currentScore;
}

int TutorialDirector::GetCurrentRequiredScore() const
{
	const TutorialTask* task = GetCurrentTask();
	if (!task) {
		return 1;
	}

	return task->requiredScore > 0 ? task->requiredScore : 1;
}

float TutorialDirector::GetCurrentProgress() const
{
	if (IsFinished()) {
		return 1.0f;
	}

	const int requiredScore = GetCurrentRequiredScore();
	if (requiredScore <= 0) {
		return 1.0f;
	}

	float progress =
		static_cast<float>(GetCurrentScore()) /
		static_cast<float>(requiredScore);

	if (progress < 0.0f) {
		progress = 0.0f;
	}
	if (progress > 1.0f) {
		progress = 1.0f;
	}

	return progress;
}

void TutorialDirector::EnterCurrentTask(const TutorialContext& context)
{
	if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(tasks_.size())) {
		return;
	}

	TutorialTask& task = tasks_[currentIndex_];
	task.currentScore = 0;

	if (task.onEnter) {
		task.onEnter(context);
	}

	currentTaskEntered_ = true;
}

void TutorialDirector::Update(const TutorialContext& context)
{
	if (!started_ || IsFinished()) {
		return;
	}

	if (!currentTaskEntered_) {
		EnterCurrentTask(context);
	}

	if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(tasks_.size())) {
		return;
	}

	TutorialTask& task = tasks_[currentIndex_];

	if (waitingComplete_) {
		completeWaitTimer_ -= context.deltaTime;

		if (completeWaitTimer_ <= 0.0f) {
			if (task.onExit) {
				task.onExit(context);
			}

			++currentIndex_;
			currentTaskEntered_ = false;
			waitingComplete_ = false;
			completeWaitTimer_ = 0.0f;
		}

		return;
	}

	int addScore = 0;
	if (task.scoreDelta) {
		addScore = task.scoreDelta(context);
	}

	if (addScore > 0) {
		task.currentScore += addScore;
	}

	if (task.currentScore >= task.requiredScore) {
		task.currentScore = task.requiredScore;

		if (task.completeWaitSeconds > 0.0f) {
			waitingComplete_ = true;
			completeWaitTimer_ = task.completeWaitSeconds;
		}
		else {
			if (task.onExit) {
				task.onExit(context);
			}

			++currentIndex_;
			currentTaskEntered_ = false;
		}
	}
}