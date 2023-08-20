#pragma once

namespace Pine
{
    class Level;
}

namespace Pine::World
{
	void SetActiveLevel(Level* level, bool ignoreLoad = false);
	Level* GetActiveLevel();
}