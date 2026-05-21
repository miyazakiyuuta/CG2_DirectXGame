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

void TutorialDirector::EnterCurrentTask(const TutorialContext& context)
{
	const TutorialTask* task = GetCurrentTask();
	if (!task) {
		return;
	}

	if (task->onEnter) {
		task->onEnter(context);
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

	const TutorialTask* task = GetCurrentTask();
	if (!task) {
		return;
	}

	if (waitingComplete_) {
		completeWaitTimer_ -= context.deltaTime;

		if (completeWaitTimer_ <= 0.0f) {
			++currentIndex_;
			currentTaskEntered_ = false;
			waitingComplete_ = false;
			completeWaitTimer_ = 0.0f;
		}

		return;
	}

	if (task->isCompleted && task->isCompleted(context)) {
		if (task->completeWaitSeconds > 0.0f) {
			waitingComplete_ = true;
			completeWaitTimer_ = task->completeWaitSeconds;
		}
		else {
			++currentIndex_;
			currentTaskEntered_ = false;
		}
	}
}