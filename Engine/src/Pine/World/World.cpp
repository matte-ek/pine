#include "World.hpp"
#include "Pine/Assets/Level/Level.hpp"

namespace 
{

	Pine::Level* m_Level = nullptr;

}

void Pine::World::SetActiveLevel(Level* level, bool ignoreLoad)
{
	if (!ignoreLoad)
	{
		level->Load();

		// We can return here since Level::Load will call this method again with ignoreLoad,
		// so m_Level will always be set.
		return;
	}

	m_Level = level;
}

Pine::Level* Pine::World::GetActiveLevel()
{
	return m_Level;
}