#pragma once

namespace Pine
{
    class Level;
}

namespace Pine::World
{
    void SetPaused(bool value);
    bool IsPaused();

    void SetTimeScale(float value);
    float GetTimeScale();

	void SetActiveLevel(Level* level, bool ignoreLoad = false);
	Level* GetActiveLevel();

    void Update();
}