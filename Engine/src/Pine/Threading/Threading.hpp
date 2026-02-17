#pragma once
#include <memory>
#include <mutex>

namespace Pine
{
    struct TaskPool;

    using TaskData = void*;
    using TaskResult = void*;

    typedef TaskResult(*TaskFunc)(TaskData);

    enum class TaskState
    {
        Queued,
        Running,
        Finished
    };

    enum class TaskThreadingMode
    {
        Default = 0,
        MainThread,
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

        TaskPool* Pool = nullptr;

        bool NotifyMainThreadUpdates = false;
    };

    struct TaskPool
    {
        bool NotifyMainThreadUpdates = false;

        std::atomic<int> JobsRemaining = 0;

        std::mutex Mutex;
        std::condition_variable ConditionVariable;
    };

    namespace Threading
    {
        void Setup();
        void Shutdown();

        TaskPool* CreateTaskPool(bool notifyMainThreadUpdates = false);
        void DeleteTaskPool(TaskPool* taskPool);

        std::shared_ptr<Task> QueueTask(TaskFunc taskFunction, TaskData data = nullptr, TaskThreadingMode mode = TaskThreadingMode::Default, TaskPool* pool = nullptr);

        TaskResult AwaitTaskResult(const std::shared_ptr<Task>& task);

        void AwaitTaskPool(TaskPool* taskPool);

        void PumpMainThreadTasks();
    }
}
