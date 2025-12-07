#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{
    class AudioFile;

    class AudioListener final : public IComponent
    {
    private:
    public:
        AudioListener();
    };

}
