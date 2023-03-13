#include <Pine/Pine.hpp>

int main()
{
    if (!Pine::Engine::Setup())
    {
        return 0;
    }

    Pine::Engine::Run();

    Pine::Engine::Shutdown();

    return 0;
}
    