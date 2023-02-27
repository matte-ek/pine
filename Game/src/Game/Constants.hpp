#pragma once

namespace Game
{

	enum GameTileFlags : std::uint32_t
	{
		PlayerSpawn = (1 << 2),
		CoinSpawn = (1 << 3),
		KeySpawn = (1 << 4),
		EnemySpawn1 = (1 << 5),
		EnemySpawn2 = (1 << 6),
		EnemySpawn3 = (1 << 7),
		ExitSpawn = (1 << 8),
	};

}