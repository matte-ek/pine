#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <variant>

namespace Pine
{
    struct TaskPool;

    using TaskResult = void*;
    using TaskFunc = std::function<TaskResult()>;

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
        // The underlying function that the threading system will call to take care
        // of whatever user work.
        TaskFunc Func = nullptr;

        TaskResult Result = nullptr;

        std::thread::id ThreadId;

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

        std::shared_ptr<Task> AddTaskToQueue(TaskFunc taskFunction, TaskThreadingMode mode = TaskThreadingMode::Default, TaskPool* pool = nullptr);

        template <typename TResult>
        std::shared_ptr<Task> QueueTask(const std::function<TResult()>& func, TaskThreadingMode mode = TaskThreadingMode::Default, TaskPool* pool = nullptr)
        {
            if constexpr (std::is_same_v<TResult, void>)
            {
                const auto wrapper = [func]() -> TaskResult
                {
                    func();
                    return nullptr;
                };

                return AddTaskToQueue(wrapper, mode, pool);
            }
            else
            {
                return AddTaskToQueue(func, mode, pool);
            }
        }

        std::shared_ptr<Task> GetCurrentTask();

        TaskResult AwaitTaskResult(const std::shared_ptr<Task>& task);

        void AwaitTaskPool(TaskPool* taskPool);

        void PumpMainThreadTasks();
    }
}
