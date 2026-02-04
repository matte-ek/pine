#pragma once

#include "Pine/Assets/Asset/Asset.hpp"
namespace Pine
{

    class InvalidAsset : public Asset
    {
    public:
        InvalidAsset();

        void Dispose() override;
    };

}