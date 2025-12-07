#pragma once

#include "Threading.hpp"

#include <condition_variable>
#include <deque>
#include <optional>

#include "Pine/Engine/Engine.hpp"

namespace
{
    bool m_IsRunning = false;

    std::vector<std::thread> m_Threads;

    std::mutex m_TaskQueueMutex;
    std::condition_variable m_TaskQueueUpdated;
    std::deque<std::shared_ptr<Pine::Task>> m_TasksQueue;

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

        lock.unlock();

        return task;
    }

    void Worker(int workerId)
    {
        Pine::Log::Verbose(fmt::format("[Threading] Worker #{} has started.", workerId + 1));

        while (m_IsRunning)
        {
            const auto& optTask = GrabTask();

            if (!optTask.has_value())
            {
                continue;
            }

            auto& task = optTask.value();

            task->State = Pine::TaskState::Running;

            task->Result = task->WorkFunction(task->WorkData);

            std::unique_lock lck(task->Mutex);

            task->State = Pine::TaskState::Finished;

            task->ConditionVariable.notify_all();
        }

        Pine::Log::Verbose(fmt::format("Threading: Worker #{} has stopped.", workerId + 1));
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

std::shared_ptr<Pine::Task> Pine::Threading::AddTask(TaskFunc taskFunction, TaskData* data)
{
    auto task = std::make_shared<Task>();

    task->WorkFunction = taskFunction;
    task->WorkData = data;

    std::unique_lock lock(m_TaskQueueMutex);

    m_TasksQueue.push_back(task);

    lock.unlock();

    m_TaskQueueUpdated.notify_one();

    return task;
}

Pine::TaskResult Pine::Threading::AwaitResult(const std::shared_ptr<Task>& task)
{
    std::unique_lock lck(task->Mutex);

    task->ConditionVariable.wait(lck, [&]{ return task->State == TaskState::Finished; });

    return task->Result;
}
