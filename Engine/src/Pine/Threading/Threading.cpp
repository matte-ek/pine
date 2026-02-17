#pragma once

#include "Threading.hpp"

#include <condition_variable>
#include <deque>
#include <optional>

#include "Pine/Engine/Engine.hpp"

namespace
{
    bool m_IsRunning = false;
    std::thread::id m_MainThreadId;

    std::vector<std::thread> m_Threads;

    std::mutex m_TaskPoolMutex;
    std::vector<Pine::TaskPool*> m_TaskPools;

    std::mutex m_TaskQueueMutex;
    std::condition_variable m_TaskQueueUpdated;
    std::deque<std::shared_ptr<Pine::Task>> m_TasksQueue;
    std::deque<std::shared_ptr<Pine::Task>> m_RunningTasks;

    std::mutex m_MainThreadQueueMutex;
    std::deque<std::shared_ptr<Pine::Task>> m_MainThreadTasksQueue;
    std::atomic m_MainThreadJobs = 0;

    // Grab the FIFO task from the queue, this call is blocking and can be waiting for either:
    // 1. We have a new task ready
    // 2. m_IsRunning == false, and we should exit.
    std::optional<std::shared_ptr<Pine::Task>> GrabTask()
    {
        std::unique_lock lock(m_TaskQueueMutex);

        m_TaskQueueUpdated.wait(lock, []{ return !m_IsRunning || !m_TasksQueue.empty(); });

        if (!m_IsRunning)
        {
            return {};
        }

        auto task = m_TasksQueue.front();

        m_TasksQueue.pop_front();
        m_RunningTasks.push_back(task);

        lock.unlock();

        return task;
    }

    void ExecuteTask(const std::shared_ptr<Pine::Task>& task)
    {
        task->State = Pine::TaskState::Running;

        task->Result = task->WorkFunction(task->WorkData);

        std::unique_lock lck(task->Mutex);

        task->State = Pine::TaskState::Finished;

        {
            std::unique_lock lock(m_TaskQueueMutex);

            for (size_t i{}; i < m_TasksQueue.size(); ++i)
            {
                if (m_RunningTasks[i] == task)
                {
                    m_RunningTasks.erase(m_RunningTasks.begin() + i);
                    break;
                }
            }
        }

        task->ConditionVariable.notify_all();
    }

    void Worker(int workerId)
    {
        Pine::Log::Verbose(fmt::format("Worker #{} has started.", workerId + 1));

        while (m_IsRunning)
        {
            const auto& optTask = GrabTask();

            if (!optTask.has_value())
            {
                continue;
            }

            auto& task = optTask.value();

            ExecuteTask(task);

            if (task->Pool != nullptr)
            {
                std::unique_lock lock(task->Pool->Mutex);
                --task->Pool->JobsRemaining;
                task->Pool->ConditionVariable.notify_all();
            }
        }

        Pine::Log::Verbose(fmt::format("Worker #{} has stopped.", workerId + 1));
    }
}

void Pine::Threading::Setup()
{
    const auto engineConfiguration = Engine::GetEngineConfiguration();

    m_IsRunning = true;

    for (int i = 0; i < engineConfiguration.m_ThreadPoolWorkers;i++)
    {
        m_Threads.emplace_back(Worker, i);
    }
}

void Pine::Threading::Shutdown()
{
    std::unique_lock lck(m_TaskQueueMutex);

    m_IsRunning = false;

    m_TaskQueueUpdated.notify_all();

    lck.unlock();

    for (auto& thread : m_Threads)
    {
        thread.join();
    }
}

Pine::TaskPool* Pine::Threading::CreateTaskPool(bool notifyMainThreadUpdates)
{
    std::unique_lock lck(m_TaskPoolMutex);

    auto pool = new TaskPool;

    pool->NotifyMainThreadUpdates = notifyMainThreadUpdates;

    m_TaskPools.push_back(pool);

    return pool;
}

void Pine::Threading::DeleteTaskPool(TaskPool* taskPool)
{
    m_TaskPools.erase(
        std::remove(m_TaskPools.begin(), m_TaskPools.end(), taskPool),
        m_TaskPools.end()
    );

    delete taskPool;
}

std::shared_ptr<Pine::Task> Pine::Threading::QueueTask(TaskFunc taskFunction, TaskData data, TaskThreadingMode mode, TaskPool* pool)
{
    auto task = std::make_shared<Task>();

    task->WorkFunction = taskFunction;
    task->WorkData = data;
    task->Pool = pool;

    if (pool != nullptr)
    {
        std::unique_lock lock(pool->Mutex);
        ++pool->JobsRemaining;
    }

    if (mode == TaskThreadingMode::Default)
    {
        std::unique_lock lock(m_TaskQueueMutex);

        m_TasksQueue.push_back(task);

        lock.unlock();

        m_TaskQueueUpdated.notify_one();
    }
    else
    {
        std::unique_lock lock(m_MainThreadQueueMutex);
        std::unique_lock lock2(m_TaskQueueMutex);

        ++m_MainThreadJobs;

        m_MainThreadTasksQueue.push_back(task);

        for (const auto& runningTask : m_RunningTasks)
        {
            runningTask->ConditionVariable.notify_all();
        }
    }

    return task;
}

Pine::TaskResult Pine::Threading::AwaitTaskResult(const std::shared_ptr<Task>& task)
{
    std::unique_lock lck(task->Mutex);

    bool isMainThread = std::this_thread::get_id() == m_MainThreadId;

    while (task->State != TaskState::Finished)
    {
        if (isMainThread)
        {
            PumpMainThreadTasks();
        }

        task->ConditionVariable.wait(lck, [&]{ return task->State == TaskState::Finished || (isMainThread && m_MainThreadJobs > 0); });
    }

    return task->Result;
}

void Pine::Threading::AwaitTaskPool(TaskPool* taskPool)
{
    std::unique_lock lck(taskPool->Mutex);

    bool isMainThread = std::this_thread::get_id() == m_MainThreadId;

    while (taskPool->JobsRemaining > 0)
    {
        if (isMainThread)
        {
            PumpMainThreadTasks();
        }

        taskPool->ConditionVariable.wait(lck, [&]
        {
            if (taskPool->NotifyMainThreadUpdates && m_MainThreadJobs > 0)
            {
                return true;
            }

            if (taskPool->JobsRemaining == 0)
            {
                return true;
            }

            return false;
        });
    }
}

void Pine::Threading::PumpMainThreadTasks()
{
    std::unique_lock lock(m_MainThreadQueueMutex);

    if (m_MainThreadTasksQueue.empty())
    {
        return;
    }

    while (!m_MainThreadTasksQueue.empty())
    {
        auto task = m_MainThreadTasksQueue.front();

        m_MainThreadTasksQueue.pop_front();

        ExecuteTask(task);

        --m_MainThreadJobs;
    }

    lock.unlock();

    std::unique_lock taskPoolLock(m_TaskPoolMutex);

    // Since pools can have `NotifyMainThreadUpdates` set, we'll notify of an update.
    for (const auto& taskPools : m_TaskPools)
    {
        if (taskPools->NotifyMainThreadUpdates)
        {
            taskPools->ConditionVariable.notify_all();
        }
    }

    taskPoolLock.unlock();
}