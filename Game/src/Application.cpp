#include <Pine/Pine.hpp>
#include <Pine/World/Components/Components.hpp>
#include <Pine/World/Components/Transform/Transform.hpp>
#include <Pine/World/Entity/Entity.hpp>

#include "AssetGenerator/AssetGenerator.hpp"
#include "Gui/Gui.hpp"

int main()
{
    if (!Pine::Engine::Setup())
    {
        return 0;
    }

    Pine::Assets::LoadDirectory("game");

    AssetGenerator::Run();

    Gui::Initialize();

    Pine::Engine::Run();

    Gui::Shutdown();

    Pine::Engine::Shutdown();

    return 0;
}
    