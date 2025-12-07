#pragma once

class b2World;

namespace Pine::Physics2D
{

	void Setup();
	void Shutdown();

	void Update(double deltaTime);

	b2World* GetWorld();

}