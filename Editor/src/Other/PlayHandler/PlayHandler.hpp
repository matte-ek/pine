#pragma once

namespace PlayHandler
{
    enum class EditorGameState
    {
        Stopped,
        Playing,
        Paused
    };

    void Play();
    void Pause();
    void Stop();

    EditorGameState GetGameState();
}