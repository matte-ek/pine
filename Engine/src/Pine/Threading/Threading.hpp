#pragma once
#include <memory>
#include <mutex>

namespace Pine
{
    using TaskData = void*;
    using TaskResult = void*;

    typedef TaskResult(*TaskFunc)(TaskData);

    enum class TaskState
    {
        Queued,
        Running,
        Finished
    };

    struct Task
    {
        // The argument we'll use for the work function.
        TaskData WorkData = nullptr;

        // The underlying function that the threading system will call to take care
        // of whatever user work.
        TaskFunc WorkFunction = nullptr;

        // Whenever the task is done, the result of `WorkFunction` will be stored here.
        TaskResult Result = nullptr;

        std::atomic<TaskState> State = TaskState::Queued;

        std::mutex Mutex;
        std::condition_variable ConditionVariable;
    };

    namespace Threading
    {
        void Setup();
        void Shutdown();

        std::shared_ptr<Task> AddTask(TaskFunc taskFunction, TaskData* data = nullptr);

        TaskResult AwaitResult(const std::shared_ptr<Task>& task);
    }
}
