#include <Pine/Pine.hpp>
#include <Pine/World/Components/Components.hpp>
#include <Pine/World/Components/Transform/Transform.hpp>
#include <Pine/World/Entity/Entity.hpp>

int main()
{
    if (!Pine::Engine::Setup())
    {
        return 0;
    }

    auto entity = Pine::Entity::Create();
    
    entity->SetActive(false);

    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}
    