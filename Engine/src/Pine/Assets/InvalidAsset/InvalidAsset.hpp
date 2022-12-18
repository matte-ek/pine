#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"
namespace Pine
{

    class InvalidAsset : public IAsset
    {
    public:
        InvalidAsset();

        void Dispose() override;
    };

}